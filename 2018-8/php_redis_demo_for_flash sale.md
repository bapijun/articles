php结合redis实现抢购demo
===
---

最开始学习redis是想要去替换关系型数据库的,不过后台写了一组demo,发现对于redis,对于复杂查询这一块实在是薄弱,尤其是对于需要落地的复杂查询,需要自己去实现索引与表关联,如果没有花大量时间去写算法和扩展,甚至无法和已经缓存过的MySQL的速度相媲美(有时间我会哪一个demo出来写一篇文章,实际上MySQL引擎,作为有大量专家进行优化的引擎,他的速度也是不容小觑的,只是因为为了acid需要落地到硬盘中所以速度会变慢).而在大量现实的使用场景中将redis作为缓存是一个更为合理的使用场景.在这个系列中我将对于redis常见的几个应用场景写下demo,并进行分析.

---

### 场景分析
为什么抢购场景适合使用redis?
- 并发数据大,数据集中热.

    在抢购场景下,对于数据的访问会即集中在三个地方.一是商品库存信息的读取,其二是商品库存的读与写,其三是对于订单的生成.<br>
    在这里的核心在于对于库存的操作,在传统的php + mysql 的框架下.抢购商品的库存必须使用锁来进行保护,否则会出现脏读和幻读的可能.

### demo

在这里我使用[phpredis](https://github.com/phpredis/phpredis)作为访问的中间件.优点在于相对于predis速度非常快,缺点在于无法直接在ide中看到注释,必须对照文档看.

    <?php
    namespace redis\generateStore;

    /**
     * 用于在redis秒杀场景下生成库存
     * @author yiwang
     *        
     */
    class GenerateStore
    {

        private $redis;
        /**
         *
         * @param array $redisOpinion redis连接参数
         */
        public function __construct($redisOpinion)
        {
            $this->redis = new \Redis();
            if ($redisOpinion['port']) {
                $this->redis->connect($redisOpinion['server'], $redisOpinion['port']);
            } else {
                $this->redis->connect($redisOpinion['server']);
            }
            //TODO 这里先只处理服务器和端口,以后可以处理其他的连接属性
        }


        function __destruct()
        {
            $this->redis->close();
            // TODO - Insert your code here
        }
        /**
         * 生成库存队列
         * @param int $storeNumber 本次秒杀的库存数
         */
        public function  generateStore($storeNumber)
        {
            for ($i = 0; $i != $storeNumber; $i++) {
                $this->redis->lPush('goods_store', 1);
                //TODO 准备插入一个完整的商品信息
            }
        }
    }

    <?php
    namespace redis\flashSale;
    use RedisLog\RedisLog;
    /**
    * @author yiwang
    *
    */
    class RushBuy
    {
    // TODO - Insert your code here

    /**
     */
    public function __construct($redisOpinion)
    {

        $this->redis = new \Redis();
        if ($redisOpinion['port']) {
            $this->redis->connect($redisOpinion['server'], $redisOpinion['port']);
        } else {
            $this->redis->connect($redisOpinion['server']);
        }
        //TODO 这里先只处理服务器和端口,以后可以处理其他的连接属性
    }

    /**
     */
    function __destruct()
    {

        // TODO - Insert your code here
    }
    /**
     * 生成唯一的订单号
     */
    private function generateOrderNumber($pre,$end='')
    {
        $yCode = array('A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J','K','L','M','N','O','P');
        $orderSn=$yCode[intval(date('Y'))-2017].strtoupper(dechex(date('m'))).date('d').substr(time(),-5).substr(microtime(),2,5).sprintf('%02d',mt_rand(0,9999));
        return $pre.$orderSn.$end;
    }

    public function rushBuy()
    {     
        $log = new RedisLog($this->redis, 'redis_log');
        if(!$this->redis->lpop('goods_store')){
            $log->info('didn\'t buy the goods');
            return;

        }
        $order = [
            'ordernumber' => $this->generateOrderNumber('O'),
            'member_id' => mt_rand(1, 100000)
        ];
        $log->info('buy the goods');
        $this->redis->hMset('rush_order:' . $order['ordernumber'], $order);
        $this->redis->lPush('rush_order_list', $order['ordernumber']);
    }

    }

### 代码解释

在这里解释我的代码<br>
在核心库存这一块我使用队列进行保存库存,原因在于对于小量的秒杀下,如果使用string保存库存,需要做两步操作一部读取库存,一步减少库存需要访问两次(这是另一个关键经验,尽量就减少PHP访问redis),当然如果对于较大数目的秒杀,我推荐使用管道或者lua脚本,来读取减少库存,而在这里使用队列.<br>
在生成订单后我将订单存入到hash中,并将order的key保存到队列中去.这样吧订单数据保存在redis中,之后就是在抢购结束之后通过脚本将热的订报数据同步到MySQL中去.从而实现整个秒杀场景.

### redis比较

### 总结
作为一个抢购demo,本demo实际上是比较简单的,尤其是订单生成和落地到MySQL中是,不过抢购的大体思路已经放在了demo中,剩下的只是进行优化根据订单的具体业务进行调整.
