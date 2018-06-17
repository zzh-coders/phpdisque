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
// var_dump($disque->ping());
// var_dump($disque->hello());
// var_dump($disque->info());
// $job_id1 = $disque->addJob('test','test_job string !!',['timeout'=>2]);
// $job_id2 = $disque->addJob('test','test_job string !!',['timeout'=>2]);

// var_dump($job_id1,$job_id2);

var_dump($disque->qlen('test'));
var_dump($disque->getJob('test',["nohang"=>true,"count"=>10]));
//var_dump($disque->ackJob($job_id1,$job_id2));
//var_dump($disque->fastAck($job_id1,$job_id2));

// var_dump($disque->delJob($job_id1,$job_id2));

// $job_id = $disque->addJob('test','test_job string !!',['timeout'=>2]);
// var_dump($disque->show($job_id));
// var_dump($disque->qlen('test'));

// var_dump($disque->qpeek('test',10));

// $job_id1 = $disque->addJob('test','test_job string !!',['timeout'=>2]);
// $job_id2 = $disque->addJob('test','test_job string !!',['timeout'=>2]);
// var_dump($disque->enqueue($job_id1,$job_id2));
// var_dump($disque->dequeue($job_id1,$job_id2));
$disque->close();
?>
