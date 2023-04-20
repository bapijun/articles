# CUDA 内存系统
本文主要是针对<cuda c编程权威指南>的总结,由于原书出版的时候cuda刚刚出到cuda6,之后的cuda版本可能有更新,可能需要我翻一翻文档,待更新.

## 内存系统架构图

![1](/2023-2/cuda_memory/1.png)

常见的内存作用域与生存期

![2](/2023-2/cuda_memory/2.png)

![3](/2023-2/cuda_memory/3.png)

## 新特性

早期的 Kepler 架构中一个颇为好用的特性就是 CUDA 程序员可以根据应用特点，自行配制 L1 Cache 和 Shared Memory 的大小。在 Volta 架构中，我们又重新引入这个特性，并且将两者的总大小做到了 128 KB。相信这对于有 Transpose，Histogram 需求的应用，或者严重依赖 L1 cache 命中率的应用都会带来不小的提高。

## 寄存器

寄存器是一个在SM中由活跃线程束划分出的较少资源。在核函数中使用较少的寄存器将使在SM上有更多的常驻线程块。每个SM上并发线程块越多，使用率和性能就越高。

如果一个核函数使用了超过硬件限制数量的寄存器，则会用本地内存替代多占用的寄存器。这种寄存器溢出会给性能带来不利影响。



## 本地内存

核函数中符合存储在寄存器中但不能进入被该核函数分配的寄存器空间中的变量将溢出到本地内存中。本地内存实际上只是全局内存,他只是一个逻辑上的概念,因此具有和全局内存一样的特性,低带宽.

编译器可能存放到本地内存中的变量有：
- 在编译时使用未知索引引用的本地数组
- 可能会占用大量寄存器空间的较大本地结构体或数组
- 任何不满足核函数寄存器限定条件的变量

## 共享内存

SM中的一级缓存和共享内存都使用64KB的片上内存,因此性能.所以与本地内存或全局内存相比，它具有更高的带宽和更低的延迟。

可以认为是可编程的缓存.

每一个SM都有一定数量的由线程块分配的共享内存。因此，必须非常小心不要过度使用共享内存，否则将在不经意间限制活跃线程束的数量。

共享内存是线程之间相互通信的基本方式。一个块内的线程通过使用共享内存中的数据可以相互合作。

## 常量内存(待补充)

目前看到的特点, 在核函数中是只读内存,作用域是全局,针对于全部设备. 同一线程块(本质上是线程束warp)线程内读相同内存地址的数据,性能很好.

常量内存是位于设备的DRAM上,对于内核代码只读的,对于主机代码是可读可写,虽然如此,其一个重要特性是具有特殊的片上缓存(这也是其高效只读性的来源),每个SM上具有64KB的缓存限制

```
常量内存驻留在设备内存中，并在每个SM专用的常量缓存中缓存。

常量变量必须在全局空间内和所有核函数之外进行声明。对于所有计算能力的设备，都只可以声明64KB的常量内存。常量内存是静态声明的，并对同一编译单元中的所有核函数可见。

核函数只能从常量内存中读取数据。因此，常量内存必须在主机端使用下面的函数来初始化.

线程束中的所有线程从相同的内存地址中读取数据时，常量内存表现最好。举个例子，数学公式中的系数就是一个很好的使用常量内存的例子，因为一个线程束中所有的线程使用相同的系数来对不同数据进行相同的计算。如果线程束里每个线程都从不同的地址空间读取数据，并且只读一次，那么常量内存中就不是最佳选择，因为每从一个常量内存中读取一次数据，都会广播给线程束里的所有线程。
```

## 纹理内存(待补充)

纹理内存驻留在设备内存中，并在每个SM的只读缓存中缓存。纹理内存是一种通过指定的只读缓存访问的全局内存，是对二维空间局部性的优化，所以使用纹理内存访问二维数据的线程可以达到最优性能。

## 全局内存

全局内存是GPU中最大、延迟最高并且最常使用的内存。它的声明可以在任何SM设备上被访问到，并且贯穿应用程序的整个生命周期。

一个全局内存变量可以被静态声明或动态声明。在主机上使用cudaMalloc分配使用cudaFee释放.

可以使用缓存来大幅度提高全局内存的性能.

```
从多个线程访问全局内存时必须注意。因为线程的执行不能跨线程块同步，不同线程块内的多个线程并发地修改全局内存的同一位置可能会出现问题，这将导致一个未定义的程序行为。

全局内存常驻于设备内存中，可通过32字节、64字节或128字节的内存事务进行访问。这些内存事务必须自然对齐，也就是说，首地址必须是32字节、64字节或128字节的倍数。优化内存事务对于获得最优性能来说是至关重要的。当一个线程束执行内存加载/存储时，需要满足的传输数量通常取决于以下两个因素：

- 跨线程的内存地址分布
- 每个事务内存地址的对齐方式在一般情况下，用来满足内存请求的事务越多，未使用的字节被传输回的可能性就越高，这就造成了数据吞吐率的降低。
```

## 固定内存

理论上固定内存不属于gpu的缓存系统,不过既然书里面写了还是放进来.

核心就是在主机端(cpu 控制下的内存),通过避免分页系统,直接使用固定内存来提升从主机传输数据的速率.建议在大规模数据传输的时候使用固定内存,也可以把许多小的传输批处理为更大的传输提高性能.

```
由于固定内存能被设备直接访问，所以它能用比可分页内存高得多的带宽进行读写。然而，分配过多的固定内存可能会降低主机系统的性能，因为它减少了用于存储虚拟内存数据的可分页内存的数量，其中分页内存对主机系统是可用的。

与可分页内存相比，固定内存的分配和释放成本更高，但是它为大规模数据传输提供了更高的传输吞吐量。

与可分页内存相比，固定内存的分配和释放成本更高，但是它为大规模数据传输提供了更高的传输吞吐量。。例如，当传输超过10MB的数据时，在Fermi设备上使用固定内存通常是更好的选择。将许多小的传输批处理为一个更大的传输能提高性能，因为它减少
了单位传输消耗。
```

## 零拷贝内存

在主机端提供统一的内存访问.集成架构中，CPU和GPU集成在一个芯片上性能较好,在离散架构中则只有优势

```
在集成架构中，CPU和GPU集成在一个芯片上，并且在物理地址上共享主存。在这种架构中，由于无须在PCIe总线上备份，所以零拷贝内存在性能和可编程性方面可能更佳。

对于通过PCIe总线将设备连接到主机的离散系统而言，零拷贝内存只在特殊情况下有优势。

因为映射的固定内存在主机和设备之间是共享的，你必须同步内存访问来避免任何潜在的数据冲突，这种数据冲突一般是由多线程异步访问相同的内存而引起的。

注意不要过度使用零拷贝内存。由于其延迟较高，从零拷贝内存中读取设备核函数可能很慢。

```

## 统一内存寻址

统一内存中创建了一个托管内存池，内存池中已分配的内存空间可以用相同的内存地址在CPU和GPU上访问。底层系统在统一内存空间中自动在主机和设备间进行数据传输，而这种传输对于应用程序而言是透明的，简化了程序代码。

统一内存寻址功能上类似零拷贝内存，但是相比于零拷贝内存他的核函数访问速率更快。同时他提供的函数的返回指针可以同时在核函数和主机代码中使用,因此提高了可读性和维护性.

## 缓存

GPU缓存
与CPU缓存类似，GPU缓存不可编程，其行为出厂是时已经设定好了。GPU上有4种缓存：

一级缓存(L1)
二级缓存(L2)
只读常量缓存(与常量内存对应)
只读缓存(与常量缓存(内存)类似,但是他的区别在于加载数据较大,相比于使用广播广播模式(同一个线程块访问相同的地址)得到更高性能的常量缓存,通常使用的只读的情况下,分散读取的时候采用只读缓存合适)

只读纹理缓存(最初只给纹理内存使用的缓存,但是在3.5算力以上的显卡上也支持给全局缓存替代,优势是相比于一级缓存他的粒度更低,32B)
每个SM都有一个一级缓存，所有SM公用一个二级缓存。一级二级缓存的作用都是被用来存储本地内存和全局内存中的数据，也包括寄存器溢出的部分。CUDA允许我们配置读操作的数据是使用一级缓存和二级缓存，还是只使用二级缓存。
与CPU不同的是，CPU读写过程都有可能被缓存，但是GPU写的过程不被缓存，只有加载会被缓存！
每个SM有一个只读常量缓存，只读纹理缓存，它们用于设备内存中提高来自于各自内存空间内的读取性能。

L1 Data-Cache：SM的私有L1 Cache和SMEM共享片上存储，他们的大小是可以配置的，L1 data cache缓存全局内存读和本地内存的读写，并且是non-corherent。本地内存主要用来寄存器溢出、函数调用和自动变量。当L1 cache 被用来缓存全局内存时是只读的，当被用来缓存本地内存时也是可写的。粒度都是128B

L2 Cache： 它被用来缓存各种类型的memory access，且和宿主机的CPU memory保持一致性。采取写回策略。全局本地内存都可以读写(加载和写入),粒度是32B,粒度更细.


关于L1, L2的更多内容可以参考[CUDA之L1、L2总结](https://blog.csdn.net/Bruce_0712/article/details/68085954)

关于缓存对于全局内存读写的影响可以参考原书