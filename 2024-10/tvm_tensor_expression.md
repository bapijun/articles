# tvm te和tir的那些关键概念和结构

最近因为研究的原因,开始做基于tvm中te(Tensor Expression)的一些内容,这里于是总结一些te相关的核心概念和数据结构.

# Tensor Expression

![1](/2024-10/te/1.webp)

TE 的全称是 Tensor Expression，是位于 Relay IR / TOPI 和 TIR 之间概念。TE 的抽象程度比 TIR 更高，且无法直接被编译为硬件源代码，必须先 lower 为 TIR 的 Primitive Function 再进行编译。

关于te,并没有找到官方的标准描述,我个人的看法是

在官方文档中[使用张量表达式表示算子](https://tvm.hyper.ai/docs/tutorial/tensor_expr/)是这样描述的.TE 用纯函数式语言描述张量计算（即每个函数表达式都不会产生副作用（side effect））。从 TVM 的整体来看，Relay 将计算描述为一组算子，每个算子都可以表示为一个 TE 表达式，其中每个 TE 表达式接收输入张量并产生一个输出张量。

我是这样理解的,作为一种提供了一层中间层语言用来方便编写TIR,从而在实现算子,甚至实现特定的irmodule.

## 关键概念

### IRModule
TVM 中，IRModule 是可以被编译的一个最小完整单元。也就是说，无论是 TIR，还是将来会介绍的 Relay IR，都必须封装到 IRModule 当中才能够被编译。或者说，IRModule 是一个待编译的完整 library，在 Linux 平台下，IRModule 编译后就是一个 .so 动态链接库。
### PrimFunc
PrimFunc 中封装了一个完整的 AST，作为 IRModule 的一个 API。IRModule 编译后，每个 PrimFunc 都对应这生成 .so 库的一个函数入口。所以一个 IRModule 可以有多个 PrimFunc。

![1](/2024-10/te/2.png)

上图是对应其他模块的primfunc的结构,严格意义上说这里提到的这些概念应该更在对应的tir层次中.

### AST

AST,抽象语法树,本质上是一个编译原理上的概念,AST 是源代码语法结构的树状表示.

TIR AST 是 TVM 中低级的中间表示，用于描述张量计算,本质上,是基于te的描述的最后就是生成基于tir语法的ast.

### TE to TIR

![3](/2024-10/te/3.webp)
上文我们提到了,te是编写tir的一层中间结构,这我们参考的[TE 的概念和编译原理](https://zhuanlan.zhihu.com/p/534313816)的案例,给出转化的逻辑.
本质上是通过te的tensor,通过各类函数提供tensor的转化,从而构造出 TIR的ast,最后生成了 的 PrimFunc，最后嵌入到新创建的 TIR IRModule 当中。

###  te.Tensor

TE 当中，tvm.te.Tensor 是指计算图中的某个数据块，概念类似于神经网络中的一个 feature map。例如，神经网络的 RGB Input 就是一个 Tensor；神经网络中 Conv、Pooling 算子的计算结果也是一个 Tensor。

上面的例子中一共出现了两种 Tensor，也是 TVM 当中最常用的 Tensor：tvm.te.placeholder 和 tvm.te.compute。

#### tvm.te.placeholder
placeholder 通常用于计算图的 Input 节点使用，没有前序节点。

#### tvm.te.compute
compute 是一种计算 Tensor，从一个或者多个前序节点接收数据，并按初始化的时候传入的 lambda 表达式计算 Tensor 内的数据。

te::compute 主要在2个地方会被调用到：
处理 topi 的时候
python 显式调用 te.compute()


这里就可以发现compute类型不再是简单的数据结构概念上的tensor,而是包含完整的运算逻辑,至于更为集体的实现,我们看下文.

通过查看te.compute函数和源代码,我们可以发发现,te.compute通过对于输入tensor的 lambda function翻译,转化为TensorComputeOp和ComputeOp.

```
    if isinstance(body, _tensor.TensorIntrinCall):
        for i, s in enumerate(shape[out_ndim:]):
            var_name = "ax" + str(i)
            dim_var.append(tvm.tir.IterVar((0, s), var_name, 4))
        op_node = _ffi_api.TensorComputeOp(
            name,
            tag,
            dim_var,
            body.reduce_axis,
            out_ndim,
            body.intrin,
            body.tensors,
            body.regions,
            body.scalar_inputs,
        )
    else:
        if not isinstance(body, (list, tuple)):
            body = [body]
        body = convert(body)
        op_node = _ffi_api.ComputeOp(name, tag, attrs, dim_var, body)
```

而这两个在C++层次中

```
/*!
 * \brief A Compute op that compute a tensor on certain domain.
 * This is the base class for ComputeOp (operating on a scalar at a time) and
 * TensorComputeOp (operating on a TensorSlice at a time)
 */
```
都是基于BaseComputeOpNode的.
这里我们看看源代码,可以看到,axis和reduce_axis是独立的模块,这也解答我在写基于te的warp gemm的一些疑惑.

注意几个关键词OperationNode,IterVar,PrimExpr.

```
class TVM_DLL BaseComputeOpNode : public OperationNode {
 public:
  /*! \brief IterVar on each axis */
  Array<IterVar> axis;
  /*! \brief IterVar on each reduction axis, if the body is a Reduce */
  Array<IterVar> reduce_axis;
  // override functions
  Array<IterVar> root_iter_vars() const final;
  Array<PrimExpr> output_shape(size_t idx) const final;
  void GatherBound(const Operation& self, const std::unordered_map<Tensor, TensorDom>& tensor_dom,
                   std::unordered_map<IterVar, Range>* out_dom_map) const final;
  Stmt BuildRealize(const Stage& stage, const std::unordered_map<IterVar, Range>& realize_map,
                    const Stmt& body, String storage_scope = "") const final;
  virtual size_t num_schedulable_dims() const = 0;

  static constexpr const char* _type_key = "BaseComputeOp";
  TVM_DECLARE_BASE_OBJECT_INFO(BaseComputeOpNode, OperationNode);
};

```
OperationNode是 TVM 中表示计算操作的基本单位
描述张量计算的具体操作 定义输入和输出的关系.

而OperationNode本身又是Operation的类,在往上则是Object,这些概念就更加抽象,可以参考paddle对于IR的基础设计的时候给出的[文档](https://github.com/PaddlePaddle/community/blob/master/pfcc/paddle-code-reading/IR_Dialect/basic_concepts.md#12-tvm-%E7%B1%BB%E5%9E%8B%E7%B3%BB%E7%BB%9F),来理解这些概念(Operation,Node,Object,type).(yysy,我读了半天还是半懂不懂, 貌似node类型,例如tensor_node之类的node类型,继承自object,而对应的tensor从objectRef继承,负责管理对应的reference)



#### IterVar
这个容易理解就是迭代器,一个IterVar可以理解为一个轴，表明在此轴上可以进行取地址.描述张量计算中的循环结构,定义计算的范围和顺序.
注释是这么说的对整数区间的迭代

```
/*!
 * \brief Iteration Variable,
 *  represents an iteration over an integer interval.
 *
 *  The dtype of the extent of the `dom` of the IterVar must match the dtype of the internal Var.
 */
```

值得注意的是在源代码中,iter和reduce iter是两个不同的数据结构保存的.

注意参数里面的dom,表示对应的范围

#### Expr和ExprNode

这里gpt给出的一组解释

Expr
Expr 是一个智能指针类（通常是 tvm::runtime::ObjectRef 的子类），用于引用表达式节点。
它提供了引用计数和类型安全的接口，使得表达式节点可以被安全地管理和操作。
Expr 主要用于在 TVM 的用户代码和 API 中传递和操作表达式。
ExprNode
ExprNode 是一个具体的表达式节点类，继承自 tvm::runtime::Object。
它定义了表达式的具体数据结构和行为，例如变量、常量、操作符等。
ExprNode 主要用于表示表达式的具体实现细节，通常不直接在用户代码中使用，而是通过 Expr 来间接操作。

也可以借鉴llvm中https://wa-lang.org/ugo-compiler-book/ch3-hello-ugo/ch3-01.html

#### PrimExpr

```
Base class of all primitive expressions.

PrimExpr is used in the low-level code optimizations and integer analysis.

```
原始（primitive）表达式,我个人的观念是,已经到了TIR的最底层结构.基于PrimExpr和op我们可以组合成更大的PrimExpr,实际上可以看出这里类似于AST上构成的概念.

在[TE 的概念和编译原理](https://zhuanlan.zhihu.com/p/534313816)的案例,给出了tir.expr.ProducerLoad的构建过程,而tir.expr.ProducerLoad本身也是PrimExpr.
![](/2024-10/te/7.webp)


TE 中也提供了控制类 PrimExpr 的封装，例如 if-then-else：

```
D = te.compute((8,), lambda i: te.if_then_else(A[i] > B[i], B[i], C[i]), name="D")

```

我们可以看到基于PrimExpr最后构成了ast,这也是compute_op在内的operation提供的output shape(原代码的定义)

#### PrimExpr可视化

通过上文TIR和TE中,我们知道了Irmodule内是有PrimFunc构成,而PrimFunc中则是PrimExpr构成的大的Ast.在我编写TE的时候一直烦恼如何可视化PrimExpr的结构,于是目前找到了两个工具,一个是tvm自带的tedd(Tensor Expression Debug Display (TEDD), visualizing Tensor Expression),这样说可能有些懵逼,我们看图

![](/2024-10/te/warptree.png)
通过这个工具,可以生成te的结构,当然这里只是调用一个函数,可以阅读文档查看提供的函数.

另一个tvm_walk提供的PrimExpr工具[PrimExprVisualizer](https://zhuanlan.zhihu.com/p/457327591#:~:text=1.%20tvm.tir),不过对应的github项目有点老,我手动改了一下代码,有机会我会放出我的版本.

![5](/2024-10/te/5.png)
我们可以看到,是如何通过PrimExpr一步步构建的语法树,到最后的PrimFun的

### STMT

TVM 中的 STMT（Statement）是指代码中的语句。通常包含或使用表达式（如 PrimExpr）

类型：
Store：存储操作
For：循环语句
IfThenElse：条件语句
Block：语句块
Evaluate：求值语句

前文中,PrimExpr的图里面,实际构成ast的对于实际是stmt,

TVM IR 的两个基础结构：Expr 和 Stmt，分别是语法表达式和语法树节点的基类。对应于在tir中expr也就是前文的primExpr,stmt也就是在tir层次中的stmt.

Stmt 的派生类有 AttrStmt（语法树属性节点）、Store（数据存储节点）、Allocate（数据 Buffer 分配节点）等等。

每个 Stmt 结构本身表示一个独立的语法树节点，但是语法树节点之间相互嵌套，通过 Stmt 的 body（Stmt 的通常结构）等成员继续向下查看就能够看到一颗完整的抽象语法树（AST）了。

我们给出一个stmt的class,可以看到它有两个部分组成,一个是PrimExpr,一个非PrimExpr的部分.非PrimExpr的部分,就是提供给语法树的其他结构和外部访问的visit方法.

```
class BufferStoreNode : public StmtNode {
 public:
  /*! \brief The buffer variable. */
  Buffer buffer;
  /*! \brief The value to be stored. */
  PrimExpr value;
  /*! \brief The indices location to be stored. */
  Array<PrimExpr> indices;
  /*! \brief The predicate mask for storing values. */
  Optional<PrimExpr> predicate;

  void VisitAttrs(AttrVisitor* v) {
    v->Visit("buffer", &buffer);
    v->Visit("value", &value);
    v->Visit("indices", &indices);
    v->Visit("predicate", &predicate);
    v->Visit("span", &span);
  }

  bool SEqualReduce(const BufferStoreNode* other, SEqualReducer equal) const {
    return equal(buffer, other->buffer) && equal(value, other->value) &&
           equal(indices, other->indices);
  }

  void SHashReduce(SHashReducer hash_reduce) const {
    hash_reduce(buffer);
    hash_reduce(value);
    hash_reduce(indices);
    hash_reduce(predicate);
  }

  static constexpr const char* _type_key = "tir.BufferStore";
  TVM_DECLARE_FINAL_OBJECT_INFO(BufferStoreNode, StmtNode);
};

```

#### STMT的构建

略,这里参考这几篇https://zhuanlan.zhihu.com/p/457337744
有点旧了,以后在读
感觉这里的内容与其说是stmt,不如说是tir的一系列内容.

这里不做展开

#### 从Tensor到PrimExpr到STMT到PrimFunc

这里给出的CreatePrimFunc函数,实际上还有基于lower的方法,这里不进行细究
可以参考[本篇](https://zhuanlan.zhihu.com/p/457327591#:~:text=1.%20tvm.tir),对应的函数.

注意这里没有涉及到pass优化

```

PrimFunc CreatePrimFunc(const Array<te::Tensor>& arg_list) {
  Array<te::Operation> arg_ops;
  for (const te::Tensor& arg : arg_list) {
    arg_ops.push_back(arg->op);
  }

  // Step 1. 将所有的 Operation 对应的 PrimExpr AST 片段连接起来
  // 构成完整的 AST Graph
  te::ReadGraph g = te::CreateReadGraph(arg_ops);

  CreateFuncInfo info(arg_list);
  Array<Stmt> root_stmts;
  arith::Analyzer analyzer;
  
  // Step 2. 采用 post-DFS 遍历 AST Graph
  Array<te::Operation> order = te::PostDFSOrder(arg_ops, g);
  for (const te::Operation& op : order) {
    // 如果是 te.placeholder，则创建一个 tir.Buffer 节点
    if (const auto* placeholder = op.as<te::PlaceholderOpNode>()) {
      const te::Tensor& tensor = op.output(0);
      const Buffer& buffer =
          decl_buffer(placeholder->shape, placeholder->dtype, placeholder->name, "global");
      info.tensor2buffers[tensor] = buffer;
    }
    // 如果是 te.compute，则创建一个 tir.Stmt 节点
    else if (const auto* compute_op = op.as<te::ComputeOpNode>()) {
      // Case 2. ComputeOp (te.compute)
      root_stmts.push_back(
          GenerateStmtFromCompute(GetRef<te::ComputeOp>(compute_op), &info, &analyzer));
    }
    // 如果是 te.extern，则创建一个 tir.Stmt 节点
    else if (const auto extern_op = op.as<te::ExternOpNode>()) {
      // Case 3. ExternOp (te.extern)
      root_stmts.push_back(GenerateStmtFromExternOp(GetRef<te::ExternOp>(extern_op), &info));
    }
  }

  // Step 3. 创建并返回 TIR PrimFunc
  PrimFunc func = WithAttrs(PrimFunc(/*params=*/std::move(parameters),
                                     /*body=*/SeqStmt::Flatten(root_stmts),
                                     /*ret_type=*/VoidType(),
                                     /*buffer_map=*/std::move(buffer_map)),
                            {{"global_symbol", String("main")}, {"tir.noalias", Bool(true)}});
  return func;
}

```



#### Type and Expr and Op
实际上这三个概念更加泛化,贯穿于IR的定义的始终,TE这个级别的IR也是有这些概念.可以参考这一篇[文章](https://zhuanlan.zhihu.com/p/446976730#:~:text=PrimExpr)


```
 IR 视为一种相对高级的编程语言，就会有两个关键的基础概念，类型 (Type) 和表达式 (Expr)。
 
 Type 包括基础的整型/浮点型等，也包括函数类型等相对复杂的类型。Expr 包括简单的定义一个字面值，也包括定义一个复杂的函数。
 
```
`
tir中op_node注释如下

```
/*!
 * \brief Primitive Op(builtin intrinsics)
 *
 * This data structure stores the meta-data
 * about primitive operators that can be invoked via Call.
 *
 * Low-level IR intrinsics(such as libc.expf) are also
 * implemented via Op.
 *
 * \sa Op
 */

```

也可以gpt等工具查询.
 

### Operation

我们现在回到tensor同一个层次的世界中.
在[compute的实现](https://zhuanlan.zhihu.com/p/534313816):
最后生成的PrimExpr(ast)打包到一个Operation（这里是 ComputeOp 类型）；然后将 Operation.output() 作为创建的 tvm.te.Tensor.
![8](/2024-10/te/8.webp)

Operation 还可以通过 Operation.InputTensors() 获得其输入 Tensor（例如 B.op.InputTensors() = [A]）

每种 Tensor 都有其对应的 Operation 类型，对应关系如下表所示

![9](/2024-10/te/8.png)

## schedule

schedule就是一系列优化选择的集合。
常见的类型参考如下:
[tvm schedule详细举例](https://zhuanlan.zhihu.com/p/94846767)

s = te.create_schedule(C.op)
```
Create a schedule for list of ops

Parameters
ops : list of Operations
The source expression.

Returns
sch : schedule.Schedule
The created schedule.

```

### Stage

一个 Stage 代表一个操作的 schedule。
一个Stage保存了op和输入的iter_var列表，iter_var就是Step1 compute中的dim_var，用来表示数据访问的顺序。

```
/********** Stage **********/
Stage::Stage(te::Operation op) {
  auto node = make_object<StageNode>();
  if (op->IsInstance<te::ComputeOpNode>()) {
    node->op_type = StageKind::kCompute;
    auto* pop = op.as<te::ComputeOpNode>();
    for (const auto& axis : pop->axis) {
      node->iters.push_back(Iterator(CleanName(axis->var->name_hint), axis->dom,
                                     IteratorKind::kSpatial, IteratorAnnotation::kNone));
    }
    for (const auto& axis : pop->reduce_axis) {
      node->iters.push_back(Iterator(CleanName(axis->var->name_hint), axis->dom,
                                     IteratorKind::kReduction, IteratorAnnotation::kNone));
    }
  } else if (op->IsInstance<te::PlaceholderOpNode>()) {
    node->op_type = StageKind::kPlaceholder;
  } else {
    LOG(FATAL) << "Unsupported operator type" << op->_type_key;
  }

  node->compute_at = ComputeAtKind::kRoot;
  node->op = std::move(op);
  node->attrs.auto_unroll_max_step = 0;
  node->attrs.storage_offset = 0;
  data_ = std::move(node);
}
```

```
/*!
 * \brief represents a stage.
 *
 *  relations form a Directed acylic hypergraph in bipartite manner.
 *  With each node is represented by a IterVar,
 *  and each hyper-edge is represented by a IterVarRelation.
 *  The relations connects the IterVars in the graph.
 *
 *  Besides typical stage that corresponds to operations.
 *  There is also group stage, which groups stages together.
 *  Each stage's group(given by group) represent an constraint,
 *  the stage can only be attached to stages within the group.
 *
 *  The group stage node can be attached to IterVars as in normal stage.
 */
class StageNode : public Object {
 public:
  /*!
   * \brief The operation of stage, can be different from original op.
   *  If it is null, then this stage is a group stage.
   */
  Operation op;
  /*!
   * \brief The original operator.
   *  The op field can change during schedule to alternate the dataflow,
   *  while origin_op remains fixed.
   */
  Operation origin_op;
  /*! \brief All the nodes in the iter var
   *
   * Each element of all_iter_vars represents an iteration variable
   * that may appear within this stage's computation.  Any element
   * of `all_iter_vars` that is in `leaf_iter_vars` represents a
   * variable that is directly defined and usable within the stage's
   * computation.  All other elements of `all_iter_vars` represent
   * variables whose value must be computed from the variables in
   * `leaf_iter_vars`.  (e.g. Support index k has been split by
   * ``ko, ki = s.split(k, factor=4)``.  ko and ki will appear in
   * `leaf_iter_vars`, while k will not, and must be computed as
   * `4*ko + ki`.
   */
  Array<IterVar> all_iter_vars;
  /*! \brief The current active leaf iter vars in the stage.
   *
   * Each element of leaf_iter_vars will either be replaced with the
   * bound index (e.g. threadIdx.x), or will be expanded into a loop
   * over the variable's extent.  `leaf_iter_vars` is a subset of
   * `all_iter_vars`.
   */
  Array<IterVar> leaf_iter_vars;
  /*!
   * \brief Specify threads to be launched at the stage.
   *  This is only valid for composite ops such as Scan.
   * \note Experimental primitive: used for thread persistence.
   */
  Array<IterVar> env_threads;
  /*!
   * \brief The predicate under which store can happen
   *  Use this when there can be duplicated threads doing the same store.
   * \note Experimental primitive: used by cross thread-reduction.
   */
  PrimExpr store_predicate;
  /*! \brief The relation bwteen of IterVars */
  Array<IterVarRelation> relations;
  /*! \brief additional attributes about iter var. */
  Map<IterVar, IterVarAttr> iter_var_attrs;
  /*! \brief The attachment type of the schedule */
  AttachType attach_type{kGroupRoot};
  /*! \brief The attach point of this schedule. */
  IterVar attach_ivar;
  /*! \brief The stage this node attaches to */
  Stage attach_stage;
  /*! \brief The schedule current stage is attached to */
  const ScheduleNode* attach_sch;
  /*! \brief The thread storage scope level of the stage */
  std::string scope;
  /*! \brief Whether this is an output stage */
  bool is_output{false};
  /*! \brief Whether apply double buffer optimization to this stage */
  bool double_buffer{false};
  /*! \brief Whether apply rolling buffer optimization to this stage */
  bool rolling_buffer{false};
  /*! \brief Layout transformations to be applied onto the stage's tensors. */
  Array<IndexMap> layout_transforms;
  /*! \brief List of axes after which to divide physical axes.
   *
   * Used to populate `BufferNode::axis_separators`, which has
   * additional details.
   */
  Array<IntImm> axis_separators;
  /*!
   * \brief The parent group of the current stage.
   *  The stage cannot be assigned to stages outside the group.
   */
  Stage group;
  /*! \brief Number of direct child stages, only used for group stage.*/
  int num_child_stages{0};

  void VisitAttrs(AttrVisitor* v) {
    v->Visit("op", &op);
    v->Visit("origin_op", &origin_op);
    v->Visit("all_iter_vars", &all_iter_vars);
    v->Visit("leaf_iter_vars", &leaf_iter_vars);
    v->Visit("env_threads", &env_threads);
    v->Visit("relations", &relations);
    v->Visit("iter_var_attrs", &iter_var_attrs);
    v->Visit("attach_type", &attach_type);
    v->Visit("attach_ivar", &attach_ivar);
    v->Visit("attach_stage", &attach_stage);
    v->Visit("scope", &scope);
    v->Visit("is_output", &is_output);
    v->Visit("double_buffer", &double_buffer);
    v->Visit("layout_transforms", &layout_transforms);
    v->Visit("axis_separators", &axis_separators);
    v->Visit("group", &group);
    v->Visit("num_child_stages", &num_child_stages);
  }

  static constexpr const char* _type_key = "Stage";
  TVM_DECLARE_FINAL_OBJECT_INFO(StageNode, Object);
};
```

https://zhuanlan.zhihu.com/p/208594323


###  schedule与compute分离

略

有机会去细读论文
http://people.csail.mit.edu/jrk/halide-pldi13.pdf

## stage

每个调度由多个阶段（Stage）组成，每个阶段表示一个运算的调度。在stage

### thread_axis
非stage的一些class的分布

## pass

不是本文的重点,有机会再聊