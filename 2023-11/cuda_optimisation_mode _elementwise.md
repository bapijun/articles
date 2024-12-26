#  Cuda 高性能计算经典模式 逐位计算(elementwise)

## 前略

写这篇文章的灵感,起源与最近几个月研究改进两个paddle 框架 cuda算子的性能,虽然最后改进约等于没有,不过还是看了大量关于cuda的优化的内容,于是打算把比较相关的三个内容(elementwise reduce presum)做成一个系列.

本篇的内容会大量应用别人写的博客和视频,更像是学习总结而不是博客.

## elementwise

elementwise,其实算是最简单的并行计算模式,逐位计算嘛,也就是具有相同结构维度的一组数据,他的每一部分的数据进行各自独立的计算,互不影响,最后产生独立的结果.

可以参考下面两张图片,注意本篇文章的图,基本都是拿的(偷得),原文应该都会在章节后面显示.

![1](/2023-11/cuda_optimisation/1-1.webp)

![2](/2023-11/cuda_optimisation/1-2.png)

这里第二章图实际上是一张跟大的图里面截取出来的,这里埋下一个扣子.下文会提.

由于这一模式如此的常见,因此在许多与cuda有关的框架都提供了类似的已经优化好的kernel函数,方便开发者进行调用.开发者需要做的是通常是这几个事情:

- 选择elementwise的类型和对应框架对应的接口函数,这里的类型值得是框架提供不同接口,例如oneflow提供了一元，二元，三元输入
- 根据选择类型,定义好出参和入参,这里比较重要的是输入输出的数据(在机器学习框架中通常是tensor)
- 编写定义好的kernel
- 备选,利用各类工具进行性能调优,修改对应的kernel

例如在pytorch中的vectorized_elementwise_kernel,Oneflow中的[cuda::elementwise::Unary](https://oneflow2020.medium.com/oneflows-optimization-of-cuda-elementwise-template-library-practical-efficient-and-extensible-b375c3bd15c6),chainer中的[chainer.cuda.elementwise](https://docs.chainer.org/en/v2.0.2/reference/util/generated/chainer.cuda.elementwise.html),cupy的[cupy.ElementwiseKernel](https://docs.cupy.dev/en/stable/reference/generated/cupy.ElementwiseKernel.html).

elementwise类的kernel其实并没有太多优化的技巧，编写合理有效的服符合官方文档中各类，以及使用好向量化访存就可以获得接近于理论极限的性能。通常情况下在框架内部,已经进行了一系列的优化,作为调用者的我们,只需要编写对应处当个数据模块的functor,只需要关注寄存器使用限制等等问题.

使用这一套模版的好处

- 性能够高，应用这套 Elementwise 模板的算子都能接近打满机器的带宽，速度也够快。
- 开发效率高，开发人员可以不用过分关注 CUDA 逻辑及相关优化手段，只需要编写计算逻辑即可。
- 可扩展性强，通常情况下框架会有自己的模板,有的甚至会提供工厂模式,自己构造新的模板。

简单地说就是开发效率和性能之间取得平衡,开发足够快,性能也很容易接近优化极限.

重点在于通过nisght compute的使用,对于用来分析具体的kernel实现瓶颈,在nsight compute中有比较深入的分析,可以得到当前kernel编写上的不足之处,进行改进.

![2](/2023-11/cuda_optimisation/1-3.png)

关于nsight compute的使用,具体可以参考[官方文档](https://docs.nvidia.com/nsight-compute/NsightCompute/index.html)

关于nsight compute的Roofline分析,可以参考这一篇[文章](https://zhuanlan.zhihu.com/p/34204282),roofline Model 提出了使用 Operational Intensity（计算强度）进行定量分析的方法，并给出了模型在计算平台上所能达到理论计算性能上限公式。

至于其他参数可以参考nsight compute的文档.

而如果是采用nsight system工具进行分析的话,我的不成熟的想法是,一般比较难得到改进的信息,核心在于一般这类的代码在整体层面的已经被框架优化到位,通过nsight system工具得到对于kernel的不足往往可以得到nsight compute的替代.

## oneflow中elementwise框架的优化

关于在框架层面elementwise使用的优化技术,在中文社区,oneflow给出好几篇有价值的文章.

这里给出他家的[OneFlow CUDA Elementwise 模板库的设计优化思路](https://zhuanlan.zhihu.com/p/447577193)

另外几篇是别人写的源代码解析
- [解析OneFlow Element-Wise 算子实现](https://zhuanlan.zhihu.com/p/591058808)
- [【CUDA编程】OneFlow Element-Wise 算子源码解读](https://zhuanlan.zhihu.com/p/646990764)

![2](/2023-11/cuda_optimisation/1-5.png)

他的核心思路包括向量化数据访问提升带宽，设置合理的Block数量（GridSize）和线程数量（BlockSize）以及在合适的地方进行循环展开（#pragma unroll）.

关于向量化他的代码里面我认为很值得一读,尤其时里面的各类c++和cuda的优化技巧.


```
我们先定义了一个 PackType 类型类型来代表向量化的数据，它代表的（向量化后的）数据大小为 sizeof(T) * pack_size。

template<typename T, int pack_size>
struct GetPackType {
  using type = typename std::aligned_storage<pack_size * sizeof(T), pack_size * sizeof(T)>::type;
};

template<typename T, int pack_size>
using PackType = typename GetPackType<T, pack_size>::type;
然后实现了一个 union 类型 Pack，它内部定义了 PackType<T, pack_size> storage; 来占用空间：

template<typename T, int pack_size>
union Pack {
  static_assert(sizeof(PackType<T, pack_size>) == sizeof(T) * pack_size, "");
  __device__ Pack() {
    // do nothing
  }
  PackType<T, pack_size> storage;
  T elem[pack_size];
};
与 storage 共享内存的，还有 T elem[pack_size]; 。这样方便后续的 Elementwise 操作：在后续计算里，我们对 elem 数组中的每个元素都应用 functor，得到输出结果。

CUDA 里最大支持128 bit 的 pack 大小，而在浮点数据类型中，最小的类型（half）大小为16 bit，最多能把128 / 16=8 个 half 数据 pack 到一起，因此我们设置了这两个常量，kMaxPackBytes 表示 pack 最大字节数，kMaxPackSize 表示 pack 数据的最大个数：

constexpr int kMaxPackBytes = 128 / 8;
constexpr int kMaxPackSize = 8;
```

关于向量化,英伟达14年的时候就给过一篇蛮有名的[博客](https://developer.nvidia.com/blog/cuda-pro-tip-increase-performance-with-vectorized-memory-access/),我在挺多地方看到过,值得大家认真阅读.

另外关于向量化的操作,特殊的数据类型,都有固定的用法,在上文的文章中,提出了针对 half2 数据类型优化,我猜测未来在英伟达给的新的cuda版本中应该会有新的类型和优化,值得关注.

关于循环展开,我记得英伟达的文档里面提过一些有意识的内容,这里挖个坑.

#### Block数量（GridSize）和线程数量（BlockSize）

其中设置合理的Block数量（GridSize）和线程数量（BlockSize）的部分他家有另一片[文章](https://zhuanlan.zhihu.com/p/442304996)的分析.

这里很推荐大家看一下这一篇文章,除了讨论GridSize的BlockSize的问题之外,其他方面的论述也很精彩.

```
要到达这个目的有多种方法，其中一个最简单的方法是让尽量多的线程同时在 SM 上执行，SM 上并发执行的线程数和SM 上最大支持的线程数的比值，被称为 Occupancy，更高的 Occupancy 代表潜在更高的性能。显然，一个 kernel 的 block_size 应大于 SM 上最大线程数和最大 block 数量的比值，否则就无法达到 100% 的 Occupancy，对应不同的架构，这个比值不相同，对于 V100 、 A100、 GTX 1080 Ti 是 2048 / 32 = 64，对于 RTX 3090 是 1536 / 16 = 96，所以为了适配主流架构，如果静态设置 block_size 不应小于 96。考虑到 block 调度的原子性，那么 block_size 应为 SM 最大线程数的约数，否则也无法达到 100% 的 Occupancy，主流架构的 GPU 的 SM 最大线程数的公约是 512，96 以上的约数还包括 128 和 256，也就是到目前为止，block_size 的可选值仅剩下 128 / 256 / 512 三个值。

... 我是省略号

还是因为 block 调度到 SM 是原子性的，所以 SM 必须满足至少一个 block 运行所需的资源，资源包括 shared memory 和寄存器， shared memory 一般都是开发者显式控制的，而如果 block 中线程的数量 * 每个线程所需的寄存器数量大于 SM 支持的每 block 寄存器最大数量，kernel 就会启动失败。

... 我是省略号

确定了 block_size 之后便可以进一步确定 grid_size，也就是确定总的线程数量，对于一般的 elementwise kernel 来说，总的线程数量应不大于总的 element 数量，也就是一个线程至少处理一个 element，同时 grid_size 也有上限，为 Maximum x-dimension of a grid of thread blocks ，目前在主流架构上都是 2^31 - 1，对于很多情况都是足够大的值

... 我是省略号

GPU 一次可以调度 SM 数量 * 每个 SM 最大 block 数个 block，因为每个 block 的计算量相等，所以所有 SM 应几乎同时完成这些 block 的计算，然后处理下一批，这其中的每一批被称之为一个 wave。想象如果 grid_size 恰好比一个 wave 多出一个 block，因为 stream 上的下个 kernel 要等这个 kernel 完全执行完成后才能开始执行，所以第一个 wave 完成后，GPU 上将只有一个 block 在执行，GPU 的实际利用率会很低，这种情况被称之为 tail effect，我们应尽量避免这种情况。将 grid_size 设置为精确的一个 wave 可能也无法避免 tail effect，因为 GPU 可能不是被当前 stream 独占的，常见的如 NCCL 执行时会占用一些 SM。所以无特殊情况，可以将 grid_size 设置为数量足够多的整数个 wave，往往会取得比较理想的结果，如果数量足够多，不是整数个 wave 往往影响也不大。

... 我是省略号


综上所述，普通的 elementwise kernel 或者近似的情形中，block_size 设置为 128，grid_size 设置为可以满足足够多的 wave 就可以得到一个比较好的结果了。

但更复杂的情况还要具体问题具体分析，比如如果因为 shared_memory 的限制导致一个 SM 只能同时执行很少的 block，那么增加 block_size 有机会提高性能.

如果 kernel 中有线程间同步，那么过大的 block_size 会导致实际的 SM 利用率降低
```

我认为可能更加完善的分析可以参考可以进一步参考官方文档与nsight compute的[官方文档](https://docs.nvidia.com/nsight-compute/NsightCompute/index.html).

在nsight compute中的会给出block size对于warp occupancy的影响的参考,倒是挺值得参考的.

![2](/2023-11/cuda_optimisation/1-4.png)

至于是否存在其他数据会影响GridSize和BlockSize对于性能的影响,这一点本人水平有限,在网上找了一下只有以下几篇合适的文章,
这一篇是红迪上关于oneflow文章的[讨论](https://www.reddit.com/r/CUDA/comments/s6ullc/how_to_choose_the_grid_size_and_block_size_for_a/
)

里面的

三个平衡问题倒是挺精彩的,可惜和这个问题关系不大

```
负载平衡/效率 -- 您希望 GPU 上的所有执行单元都能尽可能多地执行任务。每个 "线程 "应承担大致相同的工作量。

同步/正确性 -- 你希望你的程序是正确的，这意味着程序的适当部分应该在不接触相同数据的情况下并行执行（或者，以尽量减少竞赛条件的方式接触相同数据）。与这个问题相关的是原子和内存屏障/内存排序的概念，它们会大大降低程序的运行速度（！！！），但却能使程序保持正确。

简洁性 -- 我们本质上都是软件工程师。我们需要确保我们的同事明白我们刚才到底做了什么。

要同时兼顾这三个概念并不容易。有不同的策略可以平衡这三个问题。例如，最简单的做法是为每个输入数据加载一个线程，或为每个输出数据加载一个线程（如像素着色器或顶点着色器）。

但简单的同时也会失去同步或负载平衡。例如，如果你的输出不平衡（即：5 号输出的计算时间是 6 号输出的 100 倍），你的程序就会很慢。如果所有输出所需的时间大致相同，你的程序就会很快。

每个输出数据一个线程也不总是可行的。也许你需要一种更困难的同步模式来完成工作。

```
这一篇的[想法](https://stackoverflow.com/questions/47822784/calculating-grid-and-block-dimensions-of-a-kernel)是在显卡计算能力限制下,由于本身计算问题需要天然定义使用kenel的线程数目和对应计算关系下,进行计算.可以参考.同时需要平衡使用的资源数目(比如内存大小)

这一篇的[讨论](https://forums.developer.nvidia.com/t/calculating-the-optimal-grid-and-block-size/24148)的上面实际问题的综合讨论,其场景实际上是2D图像处理问题.可惜案例中并没有出现资源限制的讨论.

这篇[文章](https://selkie.macalester.edu/csinparallel/modules/CUDAArchitecture/build/html/2-Findings/Findings.html)时间比较早,给了几个规则,感觉属于老生常谈的话题,姑且可以看看.里面的图倒是不错.

![2](/2023-11/cuda_optimisation/1-6.png)


## 核函数的优化

挖个坑代填


## 文章参考与推荐阅读

这里应该是本文的重点,前面的几篇推荐重点阅读

[深入浅出GPU优化系列：elementwise优化及CUDA工具链介绍](https://zhuanlan.zhihu.com/p/488601925)

[高效、易用、可拓展我全都要：OneFlow CUDA Elementwise 模板库的设计优化思路](https://zhuanlan.zhihu.com/p/447577193)

对应的英文版[OneFlow’s Optimization of CUDA Elementwise Template Library: Practical, Efficient, and Extensible](https://oneflow2020.medium.com/oneflows-optimization-of-cuda-elementwise-template-library-practical-efficient-and-extensible-b375c3bd15c6)

[如何设置CUDA Kernel中的grid_size和block_size？](https://zhuanlan.zhihu.com/p/442304996)

[Roofline Model与深度学习模型的性能分析](https://zhuanlan.zhihu.com/p/34204282)

[A very short intro to the Roofline model](https://www.youtube.com/watch?v=IrkNZG8MJ64&ab_channel=NHR%40FAU)

[CUDA Pro Tip: Increase Performance with Vectorized Memory Access](https://developer.nvidia.com/blog/cuda-pro-tip-increase-performance-with-vectorized-memory-access/)


[介绍三个高效实用的CUDA算法实现（OneFlow ElementWise模板，FastAtomicAdd模板，OneFlow UpsampleNearest2d模板）](https://zhuanlan.zhihu.com/p/597435971)

[解析OneFlow Element-Wise 算子实现](https://zhuanlan.zhihu.com/p/591058808)

[【CUDA编程】OneFlow Element-Wise 算子源码解读](https://zhuanlan.zhihu.com/p/646990764)

[how_to_choose_the_grid_size_and_block_size](https://www.reddit.com/r/CUDA/comments/s6ullc/how_to_choose_the_grid_size_and_block_size_for_a/
)

[Calculating the optimal grid and block size?](https://forums.developer.nvidia.com/t/calculating-the-optimal-grid-and-block-size/24148)

[Choosing the right Dimensions](https://selkie.macalester.edu/csinparallel/modules/CUDAArchitecture/build/html/2-Findings/Findings.html)

[GPU Acceleration in Python Using Elementwise Kernels](https://www.nvidia.com/en-us/on-demand/session/gtcspring21-s31651/)

[nsight compute 官方文档](https://docs.nvidia.com/nsight-compute/NsightCompute/index.html)






