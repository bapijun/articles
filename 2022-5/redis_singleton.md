## php的redis单例模式

最近刚好有点时间重构了一下公司的代码,某些地方用设计模式重构了一下.刚好注意到redis的获取的生成很适合用单例模式.当然主要原因还是公司的代码框架但是用的比较老,只能自己手写设计模式.

### 单例模式

单例模式自然不用过多科普.因为是php并且公司目前不需要额外考虑多线程之类的问题,所以这里使用的是最简单的模型.参考的是这篇文章[php单例模式](https://www.cnblogs.com/wwjchina/p/7724271.html)和设计模式：可复用面向对象软件的基础这本书里面单例的设计.

如果是其他的场景下,单例模式还需要考虑多线程安全的加锁问题,请自行搜索

### 代码

这里面比较需要注意的地方,首先是通过静态的变量$instance保存单例.同时通过私有化构造函数和clone方法,使得只允许采用get_redis方法获取单例.

在get_redis中先检查是否构造过单例,没构造就构造.然后返回的是已经找构造函数中连接过的redis实例.

```

class Redis_Model
{
    /**
     * 本地保存的单例
     * @var unknown
     */
   static private $instance;
    /**
     * 本地保存的redis实例
     * @var object
     */
    static private $redis;
    //防止直接创建对象
    private function __construct($config){
        $redis = new \Redis();
        //连接redis
        $redis->connect($config['redis_host'], $config['redis_port']);
        $redis_password= $config['redis_password'];
        if ($redis_password) {
            $redis->auth($redis_password);
        }
        self::$redis = $redis;
     
    }
    //防止克隆对象
    private function __clone(){
        
    }
    static public function get_redis($config){
        //判断$instance是否是Uni的对象
        //没有则创建
        if (!self::$instance instanceof self) {
            self::$instance = new self($config);
        }
        //注意这里的返回是redis而不是object本身,这是因为我们只需要绑定在本单例下的redis对象
        return self::$redis;
        
    }
    
}
```