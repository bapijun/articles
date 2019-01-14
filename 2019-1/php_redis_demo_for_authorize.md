php+redis实现用户权限判断
===
---

最近刚好有同事在写一个基于redis的权限模块.最开始我们这个模块使用数据库保存权限,但是同事说如果用户请求大的,每一次请求都要访问数据库的数据库可能会扛不住.不如把权限放到redis里面.我redis写的多,就帮他写出这个代码.

对应的代码中有三个文件,[trait.php](1-14/redisBase.php),[Auth.php](1-14/RedisAuth.php)两个代码文件,[test.php](1-14/redisAuthTest.php)一个测试文件

---
===


### redisBase

首先我写了一个redis连接的[trait](1-14/redisBase.png),这个是把之前写的几个redis类的连接方法抽取出来了,放在构造函数,再扔到一个trait里面,这样之后写redis的模块,直接放这个trait就好.如果有多个连接池或者远程redis服务器,就修改这个trait.
```
/**
 * 通用的redisbase trait 链接数据库
 * @author bapijun
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
```

### 获取权限函数
首先获取从其他地方(我们这个项目是mysql)获取权限.
```
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
```
这里我用了闭包函数作为,每一次用闭包函数作为从数据库还是其他什么地方获取的函数方法.在类里面用成员$authorizeFunctiuon,保存这个闭包函数.而这个闭包函数是获取用户id,当然你也可以写成token.

为什么写闭包函数,因为我们不知道到底是从什么环境里面得到用户权限.所以直接把这个获取方法写成参数的形式.

我在这里类里面还写了另一个函数setAuthorzeFunction,保存权限函数,这样这个闭包方法写一次就好,之后直接就可以调用.

### 权限判断
```
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
```
这是我写的权限判断函数,$checkAuthorize, 这里的逻辑就比较简单了,首先判断是否导入新的获取权限的方法.之后检查当前用户的redis中是否权限,如果没有就去闭包方法获取权限,并保存到缓存中.
```
/**
 * 获取用户权限并保存到缓存中
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

```
最后是判断权限是否成立.这个方法可以自己修改,我这里是我们项目里面用字符串中是否存在权限判断.

### 使用方法

```
$redis = new RedisAuth();
$id = 1;
$config = ['database_type' => 'mysql',
    'database_name' => '*****',
    'server' => 'localhost',

    'port' => 3306,
    'username' => 'user',
    'password' => '*********',

    'charset' => 'utf8',
    'option' => array(
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION
    )];
$redis->setAuthorzeFunction(function ($id) use ($config) {
    $db = new Medoo($config);
    return $db->get('tb_admin_department', 'authorizelist', ['tbid' => $id]);
});

echo $redis->checkAdminAuthorize('order.browse', 1);

```
其实没什么好说的,主要是看看闭包方法这么写,这里使用use获取外部的配置.然后在mysql中获取.我这里用了一个轻量级的数据库访问类![medoo](https://medoo.lvtao.net/)
