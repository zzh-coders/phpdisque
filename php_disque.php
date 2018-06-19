<?php
$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('disque')) {
	dl('disque.' . PHP_SHLIB_SUFFIX);
}

$disque = new Disque();
try{
	$disque->connect('127.0.0.1',7711,3);
}catch(DisqueException $e){
	echo $e->getMessage();
	exit();
}
$queue_name = 'test';
$len = $disque->qlen($queue_name);
$info = $disque->getJob($queue_name,['nohang'=>true,"count"=>$len]);

$ids = array_column($info, "job_id");
$disque->ackJob(...$ids);
$disque->delJob(...$ids);
var_dump($disque->qlen($queue_name));
// exit;


var_dump('ping_cmd',$disque->ping());
var_dump('hello_cmd',$disque->hello());
var_dump('info_cmd',$disque->info());
var_dump('add_job');
$job_id = $disque->addJob($queue_name,'test_job string !!',['timeout'=>2]);
if(strlen($job_id)!=40){
	var_dump('addjob fail');
}
$disque->addJob($queue_name,'test_job string !!',['timeout'=>2]);

$len = $disque->qlen($queue_name);
if($len!=2){
	var_dump('addjob num error');
}

$list = ($disque->getJob($queue_name,["nohang"=>true,"count"=>2]));
$ids = array_column($list,'job_id');
if(count($ids) !=2){
	var_dump('getjob fail');
}
$n_len = $disque->ackJob(...$ids);

if($len!=$n_len){
	var_dump('ackjob fail||| len:'.$len.'||n_len:'.$n_len);
}
var_dump('len:'.$disque->qlen($queue_name));
exit;
var_dump($disque->fastAck($job_id1,$job_id2));

var_dump($disque->delJob($job_id1,$job_id2));

$job_id = $disque->addJob('test','test_job string !!',['timeout'=>2]);
var_dump($disque->show($job_id));
var_dump($disque->qlen('test'));

var_dump($disque->qpeek('test',10));

$job_id1 = $disque->addJob('test','test_job string !!',['timeout'=>2]);
$job_id2 = $disque->addJob('test','test_job string !!',['timeout'=>2]);
var_dump($disque->enqueue($job_id1,$job_id2));
var_dump($disque->dequeue($job_id1,$job_id2));
$disque->close();
?>
