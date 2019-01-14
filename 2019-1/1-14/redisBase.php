<?php
namespace inc;
/**
 * 通用的redisbase trait 用于大多数redis方法
 * @author yiwang
 *
 */
trait redisBase {
    /*
     * 当前的redis
     */
    private $redis;
    function __construct($redis = [])
    {
        //$this->redis = $redis;
        $this->redis = new \Redis();
        $this->redis->connect('127.0.0.1');
    }
}
