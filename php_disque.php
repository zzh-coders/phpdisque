<?php
$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('php_disque')) {
	dl('php_disque.' . PHP_SHLIB_SUFFIX);
}

$disque = new Disque();
try{
	$disque->connect('127.0.0.1',7711,3);
}catch(DisqueException $e){
	echo $e->getMessage();
	exit();
}
var_dump($disque->ping());
var_dump($disque->hello());
$disque->close();
?>
