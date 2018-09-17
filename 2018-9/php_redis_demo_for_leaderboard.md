php+redis实现排行榜demo
===
---

本周上班因为任务分配的原因,跑回去写redis去了.本周继续复习redis,感觉对于redis的实践开始有了新的认识.核心依旧是作为缓存,而不是拿关系型数据库来用.扯远了,本周又写了一些模块,其中比较好用的是使用redis来实现排行榜,确实好用,快的多,要比传统上使用mysql,存入到数据库中(当然如果能有效利用缓存也能吧速度提升一个数量级,但是还是不如redisdemo稳定),再利用mysql引擎排序来快的多,缺点是需要独立一个zset来保存排行榜.

---
===

### 有序集合和跳跃表
提到排行榜的数据结构,最开始毫无疑问我是想到当初写的大根堆,这样的二叉树实现的数据结构的[堆排序](https://idisfkj.github.io/2016/06/19/%E7%AE%97%E6%B3%95-%E4%B8%83-%E5%A0%86%E6%8E%92%E5%BA%8F/),从而实现排行榜.不过在redis中使用了更为简单的[跳跃表](https://redisbook.readthedocs.io/en/latest/internal-datastruct/skiplist.html) (当然在工程上通常会更复杂一点,这一类算法,通常会在较小的集合上用另一种简单的数据结构代替复杂数据结构)来实现有序集合.通过有序集合(zset)我们可以实现排行榜这样高效排序.

### 功能实现和分析

通过有序集合保存对应的分数和节点,这是一个非常简单的数据结构.redis的zset提供了众多的接口函数来实现对应的功能.

在这里我有两个实现思路.
- 一个是对于需要所有节点都在排行榜里面的,会将所有的节点都放入zset中
- 只需要特定数量的排行榜.比如我只需要前20件热销商品(或者说这一类需求最集中),这样我只维护一个大小为20的zset,对于最新的分数,会尝试重新插入现有的集合中,并除去最低或者最高的分数.理论上这个算法,在总分数的数量极大之后会更快.

当然在这里我先实现的第一种思路,这种思路比较简单,所有的插入更新都直接使用zset更新.如果以后有新的需求我会尝试使用第二个思路.

在这里只实现两个功能,
- 能够查询每个节点的分数和名次 通过zRevRange函数获取；
- 能够按名次查询排名前N名的节点通过zRevRank函数获取；

### 代码

这里使用[phpredis](https://github.com/phpredis/phpredis#zrangebyscore-zrevrangebyscore)扩展来实现redis的链接
```
<?php
namespace Leaderboard;

/**
 * 使用rediszset的的商品排行榜
 * @author yiwang
 *        
 */
class RedisLeaderboard
{

    /**
     *
     * @var object redis client
     */
    private $redis;
    /**
     *
     * @var string 放置排行榜的key
     */
    private $leaderboard;

    /**
     * 构造函数
     * @param object $redis 已连接redis的phpredis的对象
     * @param string $leaderboard 字符串,排行榜的key名
     */
    public function __construct($redis = [], $leaderboard = '')
    {

        if ($redis) {
          $this->redis = $redis;
        } else {
          $this->redis = new \Redis();
          $this->redis->connect('127.0.0.1');
        }

        if ($leaderboard) {
            //这里不会检查当前的key值是否存在,是为了方便重新访问对应的排行榜
            $this->leaderboard = $leaderboard;
        } else {
            $this->leaderboard = 'leaderboard:' . mt(1, 100000);
            while (!empty($this->redis->exists($this->leaderboard))) {
                $this->leaderboard = 'leaderboard:' . mt(1, 100000);
            }
        }

    }
    /**
     * 获取当前的排行榜的key名
     * @return string
     */
    public function getLeaderboard()
    {
        return $this->leaderboard;
    }
    /**
     * 将对应的值填入到排行榜中
     * @param  $node 对应的需要填入的值(比如商品的id)
     * @param number $count 对应的分数,默认值为1
     * @return Long 1 if the element is added. 0 otherwise.
     */
    public function addLeaderboard($node, $count = 1)
    {
        return $this->redis->zAdd($this->leaderboard, $count, $node);
    }
    /**
     * 给出对应的排行榜
     * @param int $number 需要给出排行榜数目
     * @param bool $asc 排序顺序 true为按照高分为第0
     * @param bool $withscores 是否需要分数
     * @param callback $callback 用于处理排行榜的回调函数
     * @return [] 对应排行榜
     */
    public function getLeadboard($number, $asc = true, $withscores = false,$callback = null)
    {
        if ($asc) {
            $nowLeadboard =  $this->redis->zRevRange($this->leaderboard, 0, $number -1, $withscores);//按照高分数顺序排行;
        } else {
            $nowLeadboard =  $this->redis->zRange($this->leaderboard, 0, $number -1, $withscores);//按照低分数顺序排行;
        }


        if ($callback) {
            //使用回调处理
            return $callback($nowLeadboard);
        } else {
            return $nowLeadboard;
        }
    }
    /**
     * 获取给定节点的排名
     * @param string $node 对应的节点的key名
     * @param string $asc 是否按照分数大小正序排名, true的情况下分数越大,排名越高
     * @return 节点排名,根据$asc排序,true的话,第一高分为0,false的话第一低分为0
     */
    public function getNodeRank($node, $asc = true)
    {
        if ($asc) {
            //zRevRank 分数最高的排行为0,所以需要加1位
            return $this->redis->zRevRank($this->leaderboard, $node);
        } else {
            return $this->redis->zRank($this->leaderboard, $node);
        }
    }

}


```
