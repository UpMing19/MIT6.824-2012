# Lab1
## Step1 : 假设网络完美情况下实现lock_server
### 1.RPC的使用（注册）
* 在lock_smain.cc中
``` c++
  server.reg(lock_protocol::stat, &ls, &lock_server::stat);
  server.reg(lock_protocol::acquire, &ls, &lock_server::acquire);
  server.reg(lock_protocol::release, &ls, &lock_server::release);
```
### 2.lockserver的设计
* 首先兼顾多线程，当有多个客户端同时请求一个锁时，保证授权一次，这里用**map**记录某个锁存不存在，如果不存在就新建一个

`
std::map<lock_protocol::lockid_t , lock*> m_lockMap;
`
* 这里的KEY = lockid_t是锁的id，单独实现lock类（也可以将lock类的中的变量放到lockserver类中）
``` c++

class lock {
public:
    enum lock_status {
        FREE,
        LOCKED
    };
    //锁id
    lock_protocol::lockid_t m_lockid;
    //锁的状态 FREE or LOCKED
    lock_status m_lockStatus;
    //条件变量
    std::condition_variable  m_cv;

    lock(lock_protocol::lockid_t lid, lock_status lockStatus);
};

```

* 这里选择用**condition_variable**和**unique_lock**方法去实现互斥资源的访问
* 也可以用
  **pthread_mutex_t mutex;** 和
  **pthread_cond_t cond;** 实现
### **acquire**()方法
1. 检查map中是否有这个锁（根据lid查询）

   `     auto it = m_lockMap.find(lid); `
2. 如果没有就新建一个并且插入到map中（注意上锁）

   `        lock *l = new lock(lid, lock::lock_status::LOCKED);
   `

   `         m_lockMap[lid] = l; `
3. 如果存在这个锁，检查是否被占用

   `     auto it = m_lockMap.find(lid); `
4. 如果被占用就排队等待（使用条件变量）

   `      while (it->second->m_lockStatus != lock::FREE) `

   `      it->second->m_cv.wait(uniqueLock); `

5. 如果没有锁是FREE状态，就获取并且修改锁的状态

   `      it->second->m_lockStatus = lock::LOCKED;`

### **release**()方法

1. 检查map中是否有这个锁（根据lid查询）

   `     auto it = m_lockMap.find(lid); `
2. 如果没有就返回对应类型

   `          return lock_protocol::NOENT; `
3. 如果存在这个锁，释放锁（使用条件变量唤醒等待的线程）

   `        it->second->m_lockStatus = lock::FREE;`

   `       it->second->m_cv.notify_all(); `




## Step2