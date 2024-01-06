# lab3 

## 1、MKDIR and UNLINK
### mkdir 
    类似于 create实现，只不过inum的第31位又1变为0（代表是目录）
### unlink 删除某一个文件
    通过字符串切割拿到 name 和 inum （注意判断是否存在和是否是文件等）
    在dir——data中删除后重新put
    同时remove掉inum的key
## 2、LOCK

  ### 在对同一目录下的文件进行修改操作时候的并发问题
    两个yfsclient 同时读取 /root/目录并且各自新建一个一个文件
    但是在yfsclient1 还没有写入更新，yfsclinet2读取到的旧内容后也开始写入，导致只添加了一个文件

### 像lock_server 获得锁之后才开始操作
    过大粒度的全局锁和过小粒度的每一个文件每一个条目的锁 都会产生较大的负面效果（死锁，开销问题等）
    
### 给每一个inum加锁
    这里給每一个路径/文件 的inum 设置一个锁
    在yfs-client中加入lock-client 传入inum作为锁id
    新建锁类
```
  class LockGuard
  {
  private:
    lock_client *lc;
    lock_protocol::lockid_t id;

  public:
    LockGuard(lock_client *lc, lock_protocol::lockid_t id) : lc(lc), id(id)
    {
      lc->acquire(id);
    }
    ~LockGuard()
    {
      lc->release(id);
    }
  };
};
```