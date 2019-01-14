<?php
namespace inc;


/**
 * redis权限类
 * @author yiwang
 *
 */
class RedisAuth
{

    use \inc\redisBase;
    /**
     * 用户的id
     * @var unknown
     */
    private $id;

    private $authorizeFunctiuon;

    /**
     * 获取用户的权限
     * @param callable $authorizeFunctiuon 回调函数,用来读取用户的权限
     * @param $id 用户id
     * @return 用户的权限
     */
    public function getAuthorize(callable $authorizeFunctiuon, $id)
    {
        return $authorizeFunctiuon($id);

    }
    /**
     * 保存用户权限
     * @param callable $authorizeFunctiuon 回调函数,用来读取用户的权限
     * @param $id 用户id
     * @return redis返回值
     */
    private function saveAuthorize(callable $authorizeFunctiuon, $id)
    {
        $authorize = $this->getAuthorize($authorizeFunctiuon, $id);
        $this->redis->set('member:authorize:' . $id  , $authorize, Array('nx', 'ex'=>24*60*60));//设置保存事件
        return $authorize;
    }

    /**
     * 设置获取权限的函数
     */
    public function setAuthorzeFunction($authorizeFunction)
    {
        $this->authorizeFunctiuon = $authorizeFunction;

    }
    /**
     * 权限判断函数
     * @param  [string] $checkAuthorize     需要判断的权限
     * @param  [int] $id                 用户id
     * @param  [callback] $authorizeFunctiuon  缓存中miss后获取权限的方法
     * @return [bool]                    是否
     */
    public function checkAdminAuthorize($checkAuthorize, $id, $authorizeFunctiuon = null)
    {
        if ($authorizeFunctiuon != null) {
            $this->authorizeFunctiuon = $authorizeFunctiuon;
        }
        $authorizeList = $this->redis->get('member:authorize:' . $id);
        if (empty($authorizeList)) {
            //如果不存在需要去读数据库
            $authorizeList = $this->saveAuthorize($this->authorizeFunctiuon, $id);
        }

        $authorizeList = explode(',', $authorizeList);
        return in_array($checkAuthorize, $authorizeList) ? true : false;
    }
}
