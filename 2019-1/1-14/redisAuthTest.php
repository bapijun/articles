<?php

use inc\RedisAuth;
use Medoo\Medoo;

require_once 'redisBase.php';
require_once 'RedisAuth.php';
require_once('vendor/autoload.php');

$redis = new RedisAuth();
$id = 1;
$config = ['database_type' => 'mysql',
    'database_name' => 'lvyoubao',
    'server' => 'localhost',

    'port' => 3306,
    'username' => 'user',
    'password' => '87663718',

    'charset' => 'utf8',
    'option' => array(
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION
    )];
$redis->setAuthorzeFunction(function ($id) use ($config) {
    $db = new Medoo($config);
    return $db->get('tb_admin_department', 'authorizelist', ['tbid' => $id]);
});

echo $redis->checkAdminAuthorize('order.browse', 1);
