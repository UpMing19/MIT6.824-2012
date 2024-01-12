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