# LBA4 lock cache
## INTRO:
  在之前的lab，如果1个yfs-client对一个目录添加文件100次，那么锁就 acquire 和 release 100次，出现不必要的性能开销，所以选择 将锁 缓存在客户端，当使用结束后并不立即release ，当有其他客户端使用的时候在release

## NEW

### lock_client_cache.{cc,h}  
    代替lock_client类,你应该在yfs-client 和 locketester 中实例化新的，他记录随机一个port用来监听服务器的rpc请求（revoke和retry）

### lock_server_cache.{cc,h}
    注册新的rpc请求
### handle.{cc,h} 和 tprintf.h:
    处理函数和DEBUG
## 设计要求
###  状态
1. NONE(客户端不知道这个锁)

2. FREE(某客户端持有锁，但是没有线程占用)

3. LOCKED(某客户端持有锁，而且某个线程正在使用)


4. ACQUIRING(正在获取锁)

5. RELEASEING(正在释放锁)


#### 1.如果一个客户端有多个线程，那么同一时间只能有一个线程占有锁，其他线程必须等待，当锁释放后（又或者锁被撤销（revoke）回服务器），唤醒等待的线程，才能获取锁
     如果需要标记线程id，可以tid = pthread_self();
#### 2A.  当锁FREE且在server的时候（不被其他client占有），client像server发送acquire请求，server回复OK，并且client占有锁

#### 2B.  当锁被不是FREE，被一个client占有且有别的client正在等待，这个时候发送acquire请求，server回复RETRY，稍后再试
#### 2C.  除以上情况外（锁在client但是没有其他client等待），这个时候发送acquire请求，server像持有锁的client发送REVOKE，等待锁被释放，最后再给正在等待客户端并return RETRY然后再return OK;


#### 3. client拿到锁时候会缓存锁，用完之后不给server发release，当同一个client的另一个线程用锁的时候，不需要与server交互就可以持有锁


#### 4.server通过发送revoke请求给client告诉他有别的client需要锁，当没有线程占用锁的时候，立即release锁给server


#### 5.服务器需要记录锁是否被持有且持有锁的每个client的ip:port（用来发送rpc请求） 和 正在等待client（给他们回复RETRY）



# Server

        FREE:锁在服务器上且空闲
        LOCKED：锁被一个客户端持有，且没有客户端等待
        LOCKED_AND_WAIT:锁被一个客户端持有，且有客户端等待
        RETRYING:锁已经释放回服务器，准备等待通知等待队列中的一个客户端获取

# Client
    NONE:锁还在服务器上，本地没有这个锁
    FREE:锁在客户端，且空闲
    LOCKED:锁在客户端被占用
    ACQUIRING：锁正在获取
    RELEASING:锁正在释放


