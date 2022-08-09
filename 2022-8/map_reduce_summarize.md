# google MapReduce总结

本文主要参考论文的[中文版](http://orange1438.github.io/pdf/Google-MapReduce.pdf),主要目的是为了个人学习阅读论文后的摘要,加强学习.

MapReduce作为分布式系统的经典之作,这篇论文很老,但是这篇论文不论在学术界还是工业界都激发了巨大的关于分布式系统的兴趣.所以6.829分布式这个课程把这篇文章作为第一篇重点阅读的文章,进行讨论.


### 编程模型

![体系结构](/2022-8/mapreduce/mapreduce_architecture.png)

以上的图片是整体的模型,最左边是输入文件,在一下的描述中会以输入键值对(k1, v1)这样的形式进行描述
右边是输出文件以list(v2)
然后是map产生的中间文件(k2, list(v2))

在论文里面是这样描述这样的过程的:
```
map(k1,v1) ->list(k2,v2)
reduce(k2,list(v2)) ->list(v2)
```


1. 其中map是接受一个输入的 key/value pair 值,然后产生一个中间 key/value pair 值的集合.

2. MapReduce库把所有具有相同中间key值I的一组中间value值(也就是对应list)在一起后传递给reduce函数.


3. Reduce函数接受2中的结果.Reduce函数合并这些value值,形成一个较小的value值的集合.一般的,我们使用[迭代器模式](https://blog.csdn.net/u011822516/article/details/40195237?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522166003664016781685363366%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fblog.%2522%257D&request_id=166003664016781685363366&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~blog~first_rank_ecpm_v1~rank_v31_ecpm-1-40195237-null-null.nonecase&utm_term=%E8%BF%AD%E4%BB%A3%E5%99%A8&spm=1018.2226.3001.4450)来节约内存和带宽.

只看名字我们就应该知道,(k1,v1) 和list(k2,v2)时不同的,但是list(k2,v2)和输出结果的list(v2)是不同的.如果用原文学术性的说法应该是推导域的问题,用C++的泛型编程的思路就是前一个的泛型是不同的,后一个泛型是共用的.




## 统计单词次数

我们以一个统计一个大的*文档集合*中每个单词出现的次数.
在这里我们定义(k1,v1)为(文档名, 文档内容),(k2,v2)为(单词, 出现频率) list(出现频率)
因此过程如下:
```
map(文档名, 文档内容) ->list(单词, 出现频率)
reduce(单词,list(出现频率)) ->list(出现频率)

```

原文给出的伪代码如下:
```
map(String key, String value):
  // key: document name
  // value: document contents
  for each word w in value:
    Emit Intermediate(w, “1”);//Mapper便会将中间结果在其本地磁盘上的存放位置报告给 Master


reduce(String key, Iterator values):
  // key: a word
  // values: a list of counts
  int result = 0;
  for each v in values:
    result += ParseInt(v);
  Emit(AsString(result));//Reduce 函数的结果会被放入到对应的 Reduce Partition 结果文件
```
这里每次找到一个单词都发射一个中间list 至于发射是什么含义,请看下文的具体描述

### 更多例子

原文给出了很多例子,这里我给了两个有趣的例子和我自己写的一个例子
- 计算 URL 访问频率Map函数处理日志中web页面请求的记录,然后输出(URL,1).Reduce 函数把相同URL的value值都累加起来,产生(URL,记录总数)结果.
- 倒排索引：Map函数分析每个文档输出一个(词,文档号)的列表,Reduce函数的输入是一个给定词的所有(词,文档号）,排序所有的文档号,输出(词,list（文档号）).所有的输出集合形成一个简单的倒排索引,它以一种简单的算法跟踪词在文档中的位置.

- 统计访问ip,map函数分析各个日志文件,输出(ip,出现频率),reduce函数把这个累加起来

这里要注意2,这里表明不仅仅只是mapreduce不仅仅是计数,也可以那来做其他的事情

### 执行概括

参考图1,我们实际的动作是和图1的序号对应

1. 作为输入的文件会被分为 M个 Split,每个 Split 的大小通常在 16~64 MB 之间如此,整个 MapReduce 计算包含 M个Map 任务和 R个Reduce 任务
2. Master 结点会从空闲的 Worker 结点中进行选取并为其分配 Map 任务和 Reduce 任务
3. 收到 Map 任务的 Worker 们（又称 Mapper）开始读入自己对应的输入文件,将读入的内容解析为输入键值对并调用由用户定义的 Map 函数.由 Map 函数产生的中间结果键值对会被暂时存放在缓冲内存区中
4. 在 Map 阶段进行的同时,Mapper 们周期性地将放置在缓冲区中的中间结果存入到自己的本地磁盘中,同时根据用户指定的 划分函数（默认为哈希方法)将产生的中间结果分为 R个部分.任务完成时,Mapper 便会将中间结果在其本地磁盘上的存放位置报告给 Master.后文会对分区函数进行描述
5. Mapper 上报的中间结果存放位置会被 Master 转发给 Reducer.当 Reducer 接收到这些信息后便会通过 RPC 读取存储在 Mapper 本地磁盘上属于对应 Partition 的中间结果.在读取完毕后,Reducer 会对读取到的数据进行排序以令拥有相同键的键值对能够连续分布
6. 之后,Reducer 会为每个键收集与其关联的值的集合,并以之调用用户定义的 Reduce 函数.Reduce 函数的结果会被放入到对应的 Reduce 结果文件
7. 当所有的 Map 和 Reduce 任务都完成之后,master 唤醒用户程序.在这个时候,在用户程序里的对MapReduce 调用才返回.

通常情况下结果并不需要合并,因为这样的R个输出文件,可以被放到分布式文件系统,或者作为另一个mapreduce程序的输入



## 技巧

本章节主要描述了原文提供的一些扩展功能

### 分区函数
在产生中间文件的的过程中,我们会采用分区函数进行划分,缺省的分区函数通常采用哈希

### 顺序保证
对于中间文件的处理是按照key值的顺序处理,这样保证了生成的输出文件是有序的.这样的顺序保证有两个好处
1. 需要对输出文件的key值随机存取的应用有很大的意义
2. 数据及的排序输出有帮助

### combiner函数
对于中间key值有很多重复的情况,原文提供了combiner函数进行优化,可以再map的本地记录进行合并,这样可以大大节约中间传输的网络带宽

### 辅助输出文件和避免副作用

在map和reduce中,有些情况下增加一些辅助输出文件,比较省事,通过writer程序,可以把这样操作变为原子化操作和幂等操作.

### 备用任务

备用任务的讨论原文是放在3容错里面的,这里我觉得放在技巧里面更合适一些
简单的说就是在要结束的时候,某些故障的或者慢速的节点会极大拖慢运行时间.因此在要结束的时候,引入备用进程,并发执行多个任务,只要有一个结束,就把任务标位结束,这样可以大大加速

### 本地执行

### 状态信息
提供了一组工具,方便监控执行状态

### 计数器
提供了一组计数器,来统计不同事件的发生次数


## 容错处理
这一章节主要讨论了如何处理机器故障
#### worker异常
master节点会周期性的ping所有的worker节点. 如果在一定的时间内没有收到worker的回复,则认为该worker已经出现异常. 当worker出现异常时,采取如下措施：

1. 该worker已完成的所有Map Task无效从设为初始的空闲状态,安排给其他的worker重新执行.
2. 该worker正在运行的Map Task或Reduce Task无效,需要重新执行.
对关于1的解释是这样的,Map阶段的输出是作为中间变量存储在local disk上的,因此当worker出现异常时,无法获取中间数据,需要重新执行.对于Reduce Task来说,其输出是直接写入分布式文件系统中的,因此无需重新执行.

关于1还需要注意一点, 当Map Task在worker A上执行完毕后,此时worker A发生故障,根据容错处理,调用worker B重新执行Map Task.此时,master结点将会通知所有的reduce worker发生异常,并让还没有从worker A上读取结果的reduce worker从worker B上读取数据.

这里对于ping模型的描述,我认为和802.11的省电模式类似

#### master异常
我们可以在master节点上周期性地进行写入磁盘,并记录监测点(checkpoint),这样当master出现故障时,我们就可以根据checkpoint来进行恢复.

#### 失效的处理机制(semantics in the presence of failure)
这里主要讨论的是如何保证即便系统出现问题的情况下,他的输出和没出问题的情况保持一致.

原文中是这样说的,如果保证map和reduce函数是确定性函数(保证相同的输入能产生一样的输出),那么就能把保证上面的论断.这里是通过map和reduce的原子性来保证的.

而对于不确定的操作,只能提供较弱但是依旧是合理的处理机制.

#### 存储位置
根据上文介绍的MapReduce整体的运行框架可以看出,在Map阶段和Reduce阶段读取数据时,都需要大量的网络传输,这成为了系统的主要性能瓶颈.为了解决这个问题,master节点会将Map Task尽量分配给离所需数据最近的worker结点.

#### 任务粒度
对于Map阶段和Reduce阶段的分区粒度M和R,文中的建议是应该远大于集群数量,原因如下,

可供每个worker执行的任务有很多种,从而更好的达到动态负载均衡
可以加速异常恢复,当某个worker结点异常时,可以把已完成的task分配到大量不同的worker上并行执行,提高并行度.
