 // 开始前向传播，标记前向函数的开始，传入序列的ID、每个序列要前向传播的长度以及可选的令牌树父指针
// \param seq_ids 即将在模型前向传播中运行的序列的ID
// \param append_lengths 每个序列要前向传播的序列长度
// \param opt_token_tree_parent_ptr 令牌树的父索引数组的可选参数，其长度为“append_lengths”的总和，若为nullptr，则意味着每个序列的令牌树是一条链
void BeginForward(const IntTuple& seq_ids, const IntTuple& append_lengths,
                    const Optional<IntTuple>& opt_token_tree_parent_ptr) final {
    // 检查序列ID的数量和追加长度的数量是否相等，若不相等则抛出错误信息
    CHECK_EQ(seq_ids.size(), append_lengths.size())
        << "The seq_ids size (" << seq_ids.size() << ") and append_lengths size ("
        << append_lengths.size() << ") mismatch.";

    // 设置当前批次大小为序列ID的数量
    cur_batch_size_ = seq_ids.size();
    // 保存当前序列的ID
    cur_seq_ids_ = seq_ids;
    // 保存当前每个序列的追加长度
    cur_append_lengths_ = append_lengths;

    // - 收集注意力计算所需的序列/块/页信息
    std::vector<Sequence*> sequences;
    std::vector<int32_t> last_block_length_before_append;
    // 标记为解码请求
    is_decode_request_ = true;
    // 预先分配内存，提高效率
    sequences.reserve(cur_batch_size_);
    last_block_length_before_append.reserve(cur_batch_size_);
    // 清空全局的k_ragged_rope_pos_offset_host_
    k_ragged_rope_pos_offset_host_.clear();
    for (int i = 0; i < cur_batch_size_; ++i) {
        // 在seq_map_中查找当前序列ID对应的序列信息
        auto it = seq_map_.find(seq_ids[i]);
        // 检查序列是否存在于KV缓存中，若不存在则抛出错误信息
        CHECK(it!= seq_map_.end()) << "The sequence \"" << seq_ids[i]
                                    << "\" cannot be found in KV cache.";
        // 将序列指针添加到sequences向量中
        sequences.push_back(&it->second);
        // 记录当前序列最后一个块在追加前的长度
        last_block_length_before_append.push_back(
            global_block_pool_[it->second.last_block_idx].seq_length);
        int k_rope_offset = it->second.seq_length;
        // 如果当前序列的接受索引未提交
        if (!it->second.accepted_indices_committed) {
            // 计算树的大小
            int tree_size = static_cast<int>(it->second.token_tree_parent_ptr.size());
            // 调整k_rope_offset
            k_rope_offset -= tree_size;
        }
        // 将k_rope_offset添加到k_ragged_rope_pos_offset_host_中
        k_ragged_rope_pos_offset_host_.push_back(k_rope_offset);
        // 更新当前序列的总长度
        it->second.seq_length += append_lengths[i];
        // 如果追加长度不为1，则不是解码请求
        if (append_lengths[i]!= 1) {
            is_decode_request_ = false;
        }
    }

    // 获取不同深度的块ID以及尾随块信息
    auto [block_ids_on_depths, trailing_blocks] = GetBlockIdsOnDepth(sequences);
    // 设置当前的深度数量为获取到的深度数量和最大允许深度的较小值
    num_depths_ =
        std::min(static_cast<int>(block_ids_on_depths.size()), kPagedKVCacheMaxBlockDepth);
    // 检查深度数量是否不超过最大允许深度
    ICHECK_LE(num_depths_, kPagedKVCacheMaxBlockDepth);

    // 用于存储分块后的块ID数组
    std::vector<std::vector<std::pair<int32_t, int32_t>>> chunked_block_ids_arr;
    // 预先分配内存
    chunked_block_ids_arr.reserve(num_depths_);
    // 用于存储是否使用解码内核的标志
    use_decode_kernel_.clear();
    for (int d = 0; d < num_depths_; ++d) {
        // 获取分块后的块ID以及是否使用解码内核的标志
        // 对于最大深度的块，不进行合并，以便与尾随的超出块进行连接
        auto [chunked_block_ids, use_decode_kernel] = GetChunkedBlockIds(
            block_ids_on_depths[d], /*enable_coalesce=*/d!= kPagedKVCacheMaxBlockDepth - 1);
        // 将分块后的块ID添加到chunked_block_ids_arr中
        chunked_block_ids_arr.push_back(chunked_block_ids);
        // 将是否使用解码内核的标志添加到use_decode_kernel_中
        use_decode_kernel_.push_back(use_decode_kernel);
    }

    // 如果深度数量达到最大允许深度
    if (num_depths_ == kPagedKVCacheMaxBlockDepth) {
        // 检查最大深度的输出块数量是否与当前批次大小相同
        CHECK_EQ(chunked_block_ids_arr[num_depths_ - 1].size(), cur_batch_size_);
    }

    // 根据是否支持滑动窗口和最后一个解码内核标志，确定是否在注意力计算前进行追加操作
    append_before_attn_ =!support_sliding_window_ && use_decode_kernel_.back();
    // 如果需要内核开始前向传播，并且查询头与键值头的比例大于等于4
    if (NeedKernelBeginForward() && num_qo_heads_ / num_kv_heads_ >= 4) {
        // 当GQA组大小至少为4且启用FlashInfer时，为了更好的性能，始终使用预填充内核
        std::fill(use_decode_kernel_.begin(), use_decode_kernel_.end(), /*value=*/false);
    }

    // 检查是否有之前的树结构
    bool has_previous_tree =
        std::any_of(sequences.begin(), sequences.end(),
                    [](const Sequence* sequence) { return!sequence->accepted_indices_committed; });
    if (has_previous_tree) {
        // 如果有之前的树结构，则在注意力计算前进行追加操作
        append_before_attn_ = true;
    }

    // - 检查令牌树的有效性并处理令牌树
    if (opt_token_tree_parent_ptr.defined()) {
        // 检查是否支持滑动窗口，若支持则抛出错误信息
        CHECK(!support_sliding_window_) << "Tree attention does not support sliding window.";
        // 检查RoPE模式，若为内联模式则抛出错误信息
        CHECK(rope_mode_!= RoPEMode::kInline) << "Tree attention does not support inline RoPE mode.";
        // 构建令牌树掩码
        ConstructTokenTreeMask(sequences, opt_token_tree_parent_ptr.value(), block_ids_on_depths,
                               trailing_blocks);
    } else {
        // 如果输入批次不构成树结构，则要求批次中的每个序列都已提交所有过去接受的令牌
        for (int i = 0; i < cur_batch_size_; ++i) {
            Sequence* sequence = sequences[i];
            // 检查序列是否已提交接受的令牌树节点，若未提交则抛出错误信息
            CHECK(sequence->accepted_indices_committed)
                << "The input batch does not form a tree, in which case the sequences in the input "
                   "batch are expected to have their accepted tokens token tree nodes committed. "
                   "Please invoke CommitAcceptedTokenTreeNodes for sequence "
                << seq_ids[i];
            // 标记当前序列为链状结构
            sequence->is_chain = true;
            // 清空令牌树父指针
            sequence->token_tree_parent_ptr.clear();
            // 清空令牌树节点深度
            sequence->token_tree_node_depths.clear();
        }
        // 将所有深度的链状结构标志设为true
        std::fill(is_chain_on_depths_.begin(), is_chain_on_depths_.end(), true);
    }

    // 如果在注意力计算前进行追加操作
    if (append_before_attn_) {
        // 当前在深度为1和不为1时使用不同的内核
        // 对于最大深度为1的情况，在追加后创建与页表相关的辅助数据结构
        for (int i = 0; i < cur_batch_size_; ++i) {
            // 为当前序列预留追加长度
            ReserveAppendLengthInSeq(sequences[i], append_lengths[i]);
        }
    }

    for (int d = 0; d < num_depths_; ++d) {
        // 获取当前深度的qo_indptr、page_indptr、page_indices、last_page_len、sliding_window_offset、sink_size和k_rope_pos_offset的主机内存向量
        HostMemoryVector& qo_indptr_h = qo_indptr_on_depths_host_[d];
        HostMemoryVector& page_indptr_h = page_indptr_on_depths_host_[d];
        HostMemoryVector& page_indices_h = page_indices_on_depths_host_[d];
        HostMemoryVector& last_page_len_h = last_page_len_on_depths_host_[d];
        HostMemoryVector& sliding_window_offset_h = sliding_window_offset_on_depths_host_[d];
        HostMemoryVector& sink_size_h = sink_size_on_depths_host_[d];
        HostMemoryVector& k_rope_pos_offset_h = k_rope_pos_offset_on_depths_host_[d];
        // 清空向量
        qo_indptr_h.clear();
        page_indptr_h.clear();
        page_indices_h.clear();
        last_page_len_h.clear();
        sliding_window_offset_h.clear();
        sink_size_h.clear();
        k_rope_pos_offset_h.clear();
        // 初始化qo_indptr和page_indptr的第一个元素为0
        qo_indptr_h.push_back(0);
        page_indptr_h.push_back(0);
        for (int i = 0; i < static_cast<int>(chunked_block_ids_arr[d].size()); ++i) {
            // 获取当前块ID和块内追加长度
            const auto& [block_id, chunk_append_length] = chunked_block_ids_arr[d][i];
            // 更新qo_indptr
            qo_indptr_h.push_back(qo_indptr_h.back() + chunk_append_length);
            if (block_id == -1) {
                // 如果块ID为-1，说明没有有效的块
                page_indptr_h.push_back(page_indptr_h.back());
                last_page_len_h.push_back(0);
                sliding_window_offset_h.push_back(0);
                sink_size_h.push_back(0);
                k_rope_pos_offset_h.push_back(0);
            } else {
                if (d < kPagedKVCacheMaxBlockDepth - 1) {
                    // 处理非最大深度的块
                    const Block& block = global_block_pool_[block_id];
                    // 更新page_indptr
                    page_indptr_h.push_back(page_indptr_h.back() + block.page_ids.size());
                    // 将块内的页ID添加到page_indices_h中
                    for (int32_t page_id : block.page_ids) {
                        page_indices_h.push_back(page_id);
                    }
                    // 计算并记录最后一页的长度
                    last_page_len_h.push_back(
                        block.seq_length == 0
                         ? 0
                          : (block.seq_length - block.sink_length + block.sliding_window_offset - 1) %
                                page_size_ +
                            1);
                    // 记录滑动窗口偏移
                    sliding_window_offset_h.push_back(block.sliding_window_offset);
                    // 记录汇聚大小
                    sink_size_h.push_back(block.sink_length);
                    // 记录k_rope_pos_offset
                    k_rope_pos_offset_h.push_back(block.start_pos);
                } else {
                    // 处理最大深度的块
                    const Block& block = global_block_pool_[block_id];
                    int32_t num_pages = static_cast<int32_t>(block.page_ids.size());
                    int32_t total_seq_length = static_cast<int32_t>(block.seq_length);
                    int32_t last_block_id = block_id;
                    // 将块内的页ID添加到page_indices_h中
                    for (int32_t page_id : block.page_ids) {
                        page_indices_h.push_back(page_id);
                    }
                    for (int32_t id : trailing_blocks[i]) {
                        // 收集尾随块（如果有）
                        const Block& block = global_block_pool_[id];
                        for (int32_t page_id : block.page_ids) {
                            page_indices_h.push_back(page_id);
                        }
                        num_pages += block.page_ids.size();
                        total_seq_length += block.seq_length;
                        last_block_id = id;
                    }
                    // 更新page_indptr
                    page_indptr_h.push_back(page_indptr_h.back() + num_pages);
                    const Block& last_block = global_block_pool_[last_block_id];
                    // 计算并记录最后一页的长度
                    last_page_len_h.push_back(total_seq_length == 0
                                                 ? 0
                                                  : (total_seq_length - last_block.sink_length +
                                                     last_block.sliding_window_offset - 1) %
                                                          page_size_ +
                                                      1);
                    // 记录滑动窗口偏移
                    sliding_window_offset_h.push_back(last_block.sliding_window_offset);
                    // 记录汇聚大小
                    sink_size_h.push_back(last_block.sink_length);
                    // 记录k_rope_pos_offset
                    k_rope_pos_offset_h.push_back(block.start_pos);
                }
            }
        }
    }

    // 如果不在注意力计算前进行追加操作
    if (!append_before_attn_) {
        // 当前在深度为1和不为1时使用不同的内核
        // 对于最大深度不为1的情况，在追加前创建与页表相关的辅助数据结构
        for (int i = 0; i < cur_batch_size_; ++i) {
            // 为当前序列预留追加长度
            ReserveAppendLengthInSeq(sequences[i], append_lengths[i]);
        }
    }

    // 映射输入批次中每个令牌位置到全局KV缓存中的位置，该映射用于追加k/v值时
    q_rope_position_map_host_.clear();
    append_position_map_host_.clear();
    for (int i = 0; i < cur_batch_size_; ++i) {
        int64_t append_length = append_lengths[i];
        const Block& block = global_block_pool_[sequences[i]->last_block_idx];
        for (int64_t pos = 0; pos < append_length; ++pos) {
            // 如果当前序列的令牌树节点深度为空
            if (sequences[i]->token_tree_node_depths.empty()) {
                // 计算并记录q_rope位置映射
                q_rope_position_map_host_.push_back(k_ragged_rope_pos_offset_host_[i] + pos);
            } else {
                // 计算树内偏移
                int64_t offset_in_tree =
                    static_cast<int64_t>(sequences[i]->token_tree_parent_ptr.size()) - append_length;
                // 检查偏移是否合法
                ICHECK_GE(offset_in_tree, 0);
                // 计算并记录q_rope位置映射
                q_rope_position_map_host_.push_back(
                    k_ragged_rope_pos_offset_host_[i] +
                    sequences[i]->token_tree_node_depths[offset_in_tree + pos]);
            }

            int32_t pos_in_block = block.seq_length - append_length + pos;
            if (last_block_length_before_append[i] + pos < block.sink_length) {
                // 要写入的位置是注意力汇聚的一部分
                int32_t offset_in_block = last_block_length_before_append[i] + pos;
                // 计算并记录追加位置映射
                append_position_map_host_.push_back(block.page_ids[offset_in_block / page_size_] *
                                                        page_size_ +
                                                    offset_in_block % page_size_);
            } else if (pos_in_block < block.sink_length) {
                // 要写入的位置在追加前被注意力汇聚固定，因此不能写入
                append_position_map_host_.push_back(-1);
            } else {
                // 要写入的位置在滑动窗口内
                int32_t offset_in_block = pos_in_block - block.sink_length + block.sliding_window_offset;
                // 计算并记录追加位置映射
                append_position_map_host_.push_back(block.page_ids[offset_in_block / page_size_] *
                                                        page_size_ +
                                                    offset_in_block % page_size_);
            }
        }
    }
}

// 结束前向传播
void EndForward() final {
    // 如果相关的前向传播结束函数未定义，则直接返回
    if (!f_attention_prefill_end_forward_.defined() ||!f_attention_decode_end_forward_.defined() ||
       !f_attention_prefill_ragged_end_forward_.defined()) {
        return;
    }
    // 调用注意力预填充的不规则结束前向传播函数
    f_attention_prefill_ragged_end_forward_.value()();
    for (int d = 0; d < num_depths_; ++d) {
        // 调用注意力预填充的结束前向传播函数
        f_attention_prefill_end_forward_.value()(d);
        // 调用注意力解码的结束前向传播函数
        f_attention_decode_end_forward_.value()(d);
    }
}