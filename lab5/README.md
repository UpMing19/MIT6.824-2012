# Lab5

## Intro
    服务器内容缓存
    将内容缓存到extent - client
    通过RPC_COUNT的数据 达到 减少get请求的 目的

## Step1 增加extent-client缓存，暂时不考虑一致性
    put():put到本地缓存
    get():如果在本地缓存有，直接返回，否则去请求服务器
    remove():清空本地缓存
    getattr():返回相应数据

## Step2
    1.在lock-client返回锁到服务器之前调用dorelease方法将yfs-client的缓存内容flush到extent-server
    2.新建extent-client的子类 extent-client_cache 并且考虑一致性的问题