<?php
namespace RedisFPC;
class RedisFPC
{
    /**
     * php redis的访问类
     * @var unknown
     */
    private $redis;

    /**
     * 构造函数
     * @param array $redis 使用phpredis的类
     * @param 是否连接成功
     */
    public function __construct($redis = [])
    {
    
        //$this->redis = $redis;
        $this->redis = new \Redis();
        return $this->redis->connect('127.0.0.1');
    }
    /**
     * 记录对应的缓存,如果之前存在则返回原本的缓存
     * @param string $cacheName 缓存名
     * @param string | callback $urlOrCallback 需要缓存的数据地址.可以是一个 网页地址也一个可回调类型,如果不是可回调类型,则判定是一个网址
     * @param null | int $ttl 缓存过期时间,如果不过期就是用默认值null
     * @throws \Exception 如果无法访问地址
     * @return boolean|string 缓存成功返回获取到的页面地址
     */
    public function remember($cacheName, $urlOrCallback, $ttl = null) 
    {
        $value = $this->get($cacheName);//检查缓存是否存在
        if (!$value) {
            //之前没有使用键
            if (is_callable($urlOrCallback)) {
                $text = $urlOrCallback();
            } else {
                //如果不是回调类型,则尝试读取网址
                $text = $this->getUrlText($urlOrCallback);
            }
            
            if (empty($text)) {
                throw new \Exception('can not get value:' . $urlOrCallback);
            }
            $this->put($cacheName, $text, $ttl);
            return $text;
        } else {
            return $value;
        }
        
    }
    /**
     * 获取对应的缓存值
     * @param string $cacheName 缓存名
     * @return String | Bool,如果不存在返回false,否则返回对应的缓存页信息
     */
    public function get($cacheName)
    {
        return $this->redis->get($this->getKey($cacheName));
    }
    /**
     * 将对应的全页缓存保存到对应redis中
     * @param string $cacheName 缓存名
     * @param string $value
     * @param null | int $ttl 过期时间,如果不过期就是用默认值null
     * @return boolean 保存成功返回true
     */
    public function put($cacheName, $value, $ttl = null)    
    {
        if (is_null($ttl)) {
            return $this->redis->set($this->getKey($cacheName), $value);
        } else {
            return $this->redis->set($this->getKey($cacheName), $value, $ttl);
        }
        
    }
    /**
     * 删除对应缓存
     * @param string $cacheName 缓存名
     */
    public function delete($cacheName)
    {
        return $this->redis->delete($this->getKey($cacheName));
    }
    
    /**
     * 更新缓存,并返回当前的缓存
     * @param string $cacheName 缓存名
     * @param string | callback $urlOrCallback 需要缓存的数据地址.可以是一个 网页地址也一个可回调类型,如果不是可回调类型,则判定是一个网址
     * @param null | int $ttl 过期时间,如果不过期就是用默认值null
     * @return boolean|string 缓存成功返回获取到的页面地址
     */
    public function refresh($cacheName, $urlOrCallback, $ttl = null)
    {
        $this->delete($cacheName);
        return $this->remember($cacheName, $urlOrCallback, $ttl);
    }
    /**
     * 获取对应的url的信息
     * @param string $url 对应的地址
     * @return boolean|string
     */
    public function getUrlText($url)
    {
        if (empty($url)) {
            return false;
        } 
        return  file_get_contents($url);
        
    }
    /**
     * 生成全页缓存键名
     * @param string $cacheName 需要缓存的名称
     * @return string 对应的在redis中的键名
     */
    private function getKey($cacheName)
    {
        return 'FPC:'. $cacheName;
    }
}