# disque操作的c扩展

官方使用命令介绍：
https://github.com/antirez/disque

目前实现的方法有以下
resource connect($host,$port,$timeout,$retry_interval)
resource pconnect($host,$port,$timeout)
bool auth($passwd)
int ackJob(string... $ids)
string addJob(string $queue, string $payload, array $options = [])
int delJob(string... $ids)
int dequeue(string... $ids)
int enqueue(string... $ids)
int fastAck(string... $ids)
array getJob(string... $queues, array $options = [])
array hello()
string info()
int qlen(string $queue)
array qpeek(string $queue, int $count) 
array show(string $id)
