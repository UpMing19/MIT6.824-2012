# lab7

## Intro
    RSM（复制状态机） 一个master 一堆replicas ，master负责从client接受命令然后在所有replicas上执行，必须要确保顺序一致，结果一致
    利用lab6实现的paxos算法来保证node的failed和re-join

    给每个请求分配viewStamp<view number(from paxos),squence number>保证顺序

    rpc服务要非阻塞

## Start
    
