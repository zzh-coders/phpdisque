<?php
$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('disque')) {
	dl('disque.' . PHP_SHLIB_SUFFIX);
}

$disque = new Disque();
try {
    $disque->pconnect('10.91.42.205', 7711, 3);
} catch (DisqueException $e) {
    echo $e->getMessage();
    exit();
}

$queue_name = 'test';
var_dump($disque->qscan([
'count'=>1
]));

var_dump($disque->jscan([
'reply'=>'all',
'state'=>["D-e4f0ed1d-6viyMlsz82hSkgPoKPJVlcsV-05a1","D-e4f0ed1d-ysGyMnxmZt9fn2+IyOcBQXng-05a1"]
]));
exit;
$len = $disque->qlen($queue_name);
$info = $disque->getJob($queue_name, ['nohang' => true, "count" => $len]);

$ids = array_column($info, "id");
$disque->ackJob(...$ids);
$disque->delJob(...$ids);
var_dump('flushdb', $disque->qlen($queue_name));


var_dump('ping_cmd', $disque->ping());
var_dump('hello_cmd', $disque->hello());
var_dump('info_cmd', $disque->info());
var_dump('add_job');
$job_id = $disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
if (strlen($job_id) != 40) {
    var_dump('addjob fail');
}
$disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
$len = $disque->qlen($queue_name);
if ($len != 2) {
    var_dump('addjob num error');
}

$list = $disque->getJob($queue_name, ["nohang" => true, "count" => $len]);
$ids = array_column($list, 'id');
if (count($ids) != 2) {
    var_dump('getjob fail');
}
$n_len = $disque->ackJob(...$ids);

if ($len != $n_len) {
    var_dump('ackjob fail||| len:' . $len . '||n_len:' . $n_len);
}
var_dump('len:' . $disque->qlen($queue_name));

var_dump('fastAck', $disque->fastAck(...$ids));

var_dump('delJob', $disque->delJob(...$ids));

var_dump('show',
    $disque->show($disque->addJob($queue_name, 'test_job string !!' . date('Y-m-d H:i:s'), ['timeout' => 2])));

var_dump('qpeek', $disque->qpeek($queue_name, 10));

$disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
$disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
$len = $disque->qlen($queue_name);
$list = $disque->getJob($queue_name, ["nohang" => true, "count" => $len]);
var_dump('last list', $list);
$ids = array_column($list, 'id');
if (count($ids) != $len) {
    var_dump('getjob fail||len:' . $len . '||n_len:' . count($ids));
}
var_dump('enqueue', $disque->enqueue(...$ids));
var_dump('dequeue', $disque->dequeue(...$ids));
var_dump('delJob', $disque->delJob(...$ids));

var_dump('working_start len', $disque->qlen($queue_name));
$job_id = $disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
var_dump('working',$disque->working($job_id));

var_dump('working_start end11', $disque->qlen($queue_name));
$disque->addJob($queue_name, 'test_job string !!', ['timeout' => 2]);
$len = $disque->qlen($queue_name);
var_dump('working_start end', $len);
$list = $disque->getJob($queue_name, ["nohang" => true, "count" => $len]);
var_dump('last list', $list);
$ids = array_column($list, 'id');
var_dump('nack', $disque->nack(...$ids));

var_dump('qstat',$disque->qstat($queue_name));

$disque->close();
unset($disque);