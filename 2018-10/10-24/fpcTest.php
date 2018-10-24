<?php 
use RedisFPC\RedisFPC;

require_once 'redisFPC.php';
/* $text = file_get_contents('http://localhost:1002/m_about.php');
var_dump($text); */
$url = 'http://localhost:1002/m_about.php';

$fpc = new RedisFPC();
echo $fpc->remember('服务协议', $url, 60*60*24);