# lab6 
    1.本实验中我们只测试 view changes 时，能够正确的启动一个新的的lock_server 并且 kill 旧的服务
    2.下个实验在验证 新server的 状态的 正确性

## INTRO
### RSM module
    负责赋值，当有新的节点加入时（向下调用config），启动在每个节点上的恢复线程，保证所有view拥有一致性，
    在这个实验中，需要恢复的是paxos的执行序列
    下个实验，复制锁服务
### config module
    view管理，向下调用paxos（当有node加入时）
    发送心跳检测，及时去除宕机节点（invoke paxos）
### Paxos module
    形成新View的一个选举过程

#### To avoid deadlock, we suggest that you use the rule that a module releases its internal mutexes before it upcalls, but can keep its mutexes when calling down.

#### 测试 rsm_tester.pl 时 ，杀死所有server进程且清空日志


## 根据伪代码完成Paxos.cc

```
proposer run(instance, v):
 choose n, unique and higher than any n seen so far
 send prepare(instance, n) to all servers including self
 if oldinstance(instance, instance_value) from any node:
   commit to the instance_value locally
 else if prepare_ok(n_a, v_a) from majority:
   v' = v_a with highest n_a; choose own v otherwise
   send accept(instance, n, v') to all
   if accept_ok(n) from majority:
     send decided(instance, v') to all

acceptor state:
 must persist across reboots
 n_h (highest prepare seen)
 instance_h, (highest instance accepted)
 n_a, v_a (highest accept seen)

acceptor prepare(instance, n) handler:
 if instance <= instance_h
   reply oldinstance(instance, instance_value)
 else if n > n_h
   n_h = n
   reply prepare_ok(n_a, v_a)
 else
   reply prepare_reject

acceptor accept(instance, n, v) handler:
 if n >= n_h
   n_a = n
   v_a = v
   reply accept_ok(n)
 else
   reply accept_reject

acceptor decide(instance, v) handler:
 paxos_commit(instance, v)
```


