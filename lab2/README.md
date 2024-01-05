﻿# yfs2012
#Yield another File System
Yield another File System用作2012年[MIT 6.824 Distributed Systems](http://pdos.csail.mit.edu/6.824-2012/index.html)
的课程实践,按照论文[Frangipani: A Scalable Distributed File System](http://pdos.csail.mit.edu/6.824-2012/papers/thekkath-frangipani.pdf)的思路实现一个分布式的文件系统. 文件系统的基本结构如下:
![yfs](https://github.com/ldaochen/yfs2012/raw/lab2/yfs.jpg)

 

 - yfs: 文件系统客户端(相对extent server),提供文件操作API.
 - extent server: 文件存储服务
 - lock server: 锁服务,保证文件系统并发访问时的行为正确
 
主要需要实现的模块和功能如下:
 1. 锁服务
 2. 文件系统服务
 3. 锁缓存和文件缓存,一致性
 4. 利用Paxos算法和Replicated State Machine方法备份锁服务.

文档后面的7节依次介绍这些功能的实现,这7节分别是:

**Lab 1 - Lock Server**

**Lab 2 - Basic File Server**

**Lab 3 - MKDIR, UNLINK, and Locking**

**Lab 4 - Caching Lock Server**

**Lab 5 - Caching Extent Server + Consistency**

**Lab 6 - Paxos**

**Lab 7 - Replicated lock server**

#Lab 1: Lock Server
##简介
整个的2012年MIT 6.824 课程实验需要完成一个分布式的文件系统, 而文件系统结构的更新需要在锁的保护下进行.所以本次实验(lab1)是为了实现一个简单的锁服务. 

锁服务的核心逻辑包含两个模块:**锁客户端**和**锁服务器**.
两个模块之间通过RPC(远程过程调用)进行通信. 当**锁客户端**请求一个特定的锁时,它向**锁服务器**发送一个**acquire请求**.**锁服务器**确保这个锁一次只被一个**锁客户端**获取. 当**锁客户端**获取到锁,完成相关的操作后,通过给**锁服务器**发送一个**release请求**,这样**锁服务器**可以使得该锁被其他等待该锁的**锁客户端**获取. 在本次实验中除了实现锁服务,还需要给RPC库增加消除重复RPC请求的功能确保*at-most-once*执行. 因为在网络环境下RPC请求可能被丢失,所以RPC系统必须重传丢失的RPC.但是有些情形下原来的RPC并没有丢失,但是RPC系统却认为已经丢失了,结果进行了重传,导致重复的RPC请求. 下面是一个例子介绍重复的RPC请求带来的不正确的行为.**锁客户端**为了获取锁X,给**锁服务器**发送一个**acquire 请求**. **锁服务器**确保锁X被该客户端获取.然后**锁客户端**通过一个**release请求**释放锁X.但是这时一个重复的RPC请求到达**锁客户端**要求获取锁X. **锁服务器**确保该锁被获取,但是**锁客户端**绝不会释放锁X. 因为这个请求只是第一次**acquire请求**的副本.
## 实验内容
实验内容分为两部分: 
1. 提供基本锁操作**acquire**和**release**.
2. 考虑重复RPC请求.消除重复RPC请求带来的错误
下面分两个部分进行介绍
### 第一部分
在这部分中不需要考虑网络带来的重复RPC,假设网络是完美的.仅仅需要实现基本的锁操作**acquire**和**release**. 并且必须遵守一个不变量:**任何时间点,同一个锁不能被两个或者以上的锁客户端持有**
下面介绍实现过程. 
lock_smain.cc包含锁服务的基本过程.其中定义一个lock_server ls,然后在RPC服务中登记各种请求对应的handler. 我们需要实现**acquire**和**release**,因此也需要在RPC服务中添加对应的handler,在lock_smain.cc中增加相应的代码,如下:
    
    lock_server ls; 
    rpcs server(atoi(argv[1]), count);
    server.reg(lock_protocol::stat, &ls, &lock_server::stat);
    server.reg(lock_protocol::acquire, &ls, &lock_server::acquire);
    server.reg(lock_protocol::release, &ls, &lock_server::release);`

**锁客户端**的请求**lock_protocol::acquire**和**lock_protocol::release**
在**锁服务器**中相应的handler是**lock_server::acquire**和**lock_server::release**. 

上面建立了RPC请求和handler的关系,但是实验给出的代码中没有给出相应的锁的定义,因此我们需要自定义锁. 在lock_protocol.h中添加锁的定义(也可以新建单独的文件在其中定义锁,但是这需要修改GNUMakefile来包含新文件).
	
	class lock {
		public: 
			enum lock_status {FREE, LOCKED};
			lock_protocol::lockid_t lid;
			int status;
			pthread_cond_t lcond;
			lock(lock_protocol::lockid_t);
			lock(lock_protocol::lockid_t, int);
			~lock(){};
	};
其中
* lid 表示锁的id,用来唯一的标示锁.
* stauts 表示锁的状态,FREE或者LOCKED.
* lcond 是一个条件变量,当锁是LOCKED状态时,其它要获取该锁的线程必须等待.
当锁的状态变为FREE时,需要唤醒这些等待的进程.

RPC系统维护了线程池,当收到一个请求后,从线程池中选择一个空闲线程执行请求对应的handler. 因此会有多个线程并发请求锁的情形. 同时**锁服务器**维护的
锁的个数是开放的,任意增长的,当**锁客户端**请求一个从未有过的锁时,就创建一个新的锁. 在lock_server.h中我们增加了一个数据结构lockmap, 记录**锁服务器**
维护的所有锁.同时一个互斥量mutex,用来保证多线程并发的访问时不会出错.
	
	class lock_server {
		protected:
			int nacquire;
			pthread_mutex_t mutex;
			std::map<lock_protocol::lockid_t, lock* > lockmap;
		public:
			lock_server();
			~lock_server() {}; 
			lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &); 
			lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &); 
			lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &); 
	};
在lock_server结构中我们还看了**acquire**和**release**的操作,这两个操作
在前面的介绍中已经作为handler登记到RPC系统中. 这两个函数参数信息包含
* clt: 锁客户端id, 用来标示**锁客户端**
* lid: 所请求的锁的id.用来表示锁
* 第三个参数是一个结果信息.
当**锁客户端**的请求到来后,RPC系统就从线程池中找一个空闲的线程执行
对应handler,可能是lock_server中的**acquire**或者**release**.
下面介绍这两个函数的实现. 在lock_server.cc中**acquire**的实现如下:
	
       
         lock_protocol::status
	    lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) 
	    {
		    lock_protocol::status ret = lock_protocol::OK;
		    std::map<lock_protocol::lockid_t, lock* >::iterator iter;
		    pthread_mutex_lock(&mutex);
		    iter = lockmap.find(lid);
		    if(iter != lockmap.end()) {
			    while(iter->second->status != lock::FREE) {
				    pthread_cond_wait(&(iter->second->lcond), &mutex);
			    }
			    iter->second->status = lock::LOCKED;
			    pthread_mutex_unlock(&mutex);
			    return ret;
		    } else {
			    lock *new_lock = new lock(lid, lock::LOCKED);
			    lockmap.insert(std::make_pair(lid, new_lock));
			    pthread_mutex_unlock(&mutex);
			    return ret;
		    }	   
	    }

因为多线程需要互斥的访问共享的数据结构lockmap.所以首先需要获取mutex.
然后在lockmap中查询lid对应的锁的状态,如果是LOCKED,那么当前线程在该锁
的条件变量lcond上阻塞直到锁被释放,但是线程将被唤醒,然后在while循环中检测锁的状态,直到可以获取该锁.如果锁的状态是FREE,就直接将锁的状态修改为LOCKED,表示获取该锁. 如果请求的锁不存在,就创建一个新锁加入到lockmap中, 并确保新创建的锁被**锁客户端**获取. 

对应**锁服务器**中**release**的实现如下
	
	lock_protocol::status
	lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) 
	{
		lock_protocol::status ret = lock_protocol::OK;
		std::map<lock_protocol::lockid_t, lock*>::iterator iter;
		pthread_mutex_lock(&mutex);
		iter = lockmap.find(lid);
		if (iter != lockmap.end()) {
			iter->second->status = lock::FREE;
			pthread_cond_signal(&(iter->second->lcond));
			pthread_mutex_unlock(&mutex);
			return ret;
		} else {
			ret = lock_protocol::IOERR;
			pthread_mutex_unlock(&mutex);
			return ret;
		}
	}
**release**的实现相对很简单,在lockmap中查询对应的锁,然后将锁的状态置为
FREE,然后唤醒等待该锁的线程.

以上是**acquire**和**release**在**锁服务器**端的实现,下面介绍这两个操作在**锁客户端**的实现.参照客户端stat的写法.在lock_client.cc中添加如下代码

	lock_protocol::status
	lock_client::acquire(lock_protocol::lockid_t lid)
	{
		int r;
		lock_protocol::status ret = cl->call(lock_protocol::acquire, cl->id(), lid, r); 
		VERIFY(ret == lock_protocol::OK);
		return r;
	}

	lock_protocol::status
	lock_client::release(lock_protocol::lockid_t lid)
	{
		int r;
		lock_protocol::status ret = cl->call(lock_protocol::release, cl->id(), lid, r); 
		VERIFY(ret == lock_protocol::OK);
		return r;
	}
到此实验的第一部分完成,可以通过lock_tester的测试.
###第二部分
这里主要考虑消除重复RPC带来的错误,确保*at-most-once*执行.
一种方法是在服务器端记录所有已经接收到的RPC请求,每个RPC请求由xid和clt_nonce标示,clt_nonce用来标示是哪个客户端,xid标示该客户端发送的特定的一个请求.除了记录RPC的标示,还需要记录每个RPC的处理结果,这样当重复的RPC请求到来时,重发这个结果就行.这种方法确保"at-most-once",但是记录这些信息的内存消耗几乎是无限增长的. 另一种方法是滑动窗口.服务器端只记录部分的RPC请求信息,而不是全部,而且要求客户端的xid必须是严格的递增,如0,1,2,3... 当服务器端接收到一个请求,该请求中包含三个信息(xid, clt_nonce, xid_rep)
xid和clt_nonce前面已经介绍,用来标示一个请求,而xid_rep的意思是告诉服务器
端xid_rep之前的请求客户端都已经收到了回复.所以服务器端不需要在保存xid_rep之前的信息了,可以删除. 服务器收到请求(xid,clt_nonce,xid_rep)后,将该请求
在窗口中查询:
1. 如果窗口中不存在这个请求,表示这是一个新请求.那么将该请求加入到窗口,同时删除xid_rep之前的请求,然后调用对应的handler处理这个新请求.
然后处理的结果存入窗口(存储结果的过程由add_reply函数完成).
2. 如果这个请求在窗口中已经存在.说明现在的请求是一个重复的请求,不需要调用handler.直接在窗口中查找这个请求的结果. 然后结果还没准备好,说明handler还在处理这个请求. 如果结果已经存在,说明handler已经处理完了这个请求,直接将结果再重发给客户端.
3. 如果窗口中不存在这个请求,并且xid小于客户端clt_nonce对应的窗口中最小的xid.说明这个请求已经被从窗口中删除.
上面三个过程由rpc/rpc.cc中checkduplicate_and_update函数完成.这部分代码
需要我们编写. 实现如下:
	
	rpcs::rpcstate_t
	rpcs::checkduplicate_and_update(unsigned int clt_nonce, unsigned int xid,
		unsigned int xid_rep, char **b, int *sz)
	{
		ScopedLock rwl(&reply_window_m_);
		std::list<reply_t>::iterator iter;
		for (iter = reply_window_[clt_nonce].begin(); iter != reply_window_[clt_nonce].end(); ) {
			if (iter->xid < xid_rep && iter->cb_present) {
				free(iter->buf);
				iter = reply_window_[clt_nonce].erase(iter);
				continue;
			}
			if (xid == iter->xid) {
				if(iter->cb_present) {
					*b = iter->buf;
					*sz = iter->sz;
					return DONE;
				} else {
					return INPROGRESS;
				}
			}
			if(reply_window_[clt_nonce].front().xid > xid)
				return FORGOTTEN;
			iter++;
		}
		reply_t reply(xid);
		for (iter = reply_window_[clt_nonce].begin(); iter != reply_window_[clt_nonce].end(); iter++) {
			if(iter->xid > xid) {
				reply_window_[clt_nonce].insert(iter, reply);
				break;
			}
		}
		if(iter == reply_window_[clt_nonce].end())
			reply_window_[clt_nonce].push_back(reply);
		return NEW;
			// You fill this in for Lab 1.
	}
iter->cb_present表示结果是否有效.如果有效则返回DONE.如果无效表示还在处理,返回INPROGRESS. 如果xid比clt_nonce对的窗口中最小的xid还要xiao,说明
这个请求已经被删除.然会FORGOTTEN. 对于新的请求则按序插入到窗口中. 
第一个for循环中已经将xid_rep前面的请求删除.

我们还需要实现另一个函数add_reply,这个函数的作用是将一个请求的结果保存在
窗口中.在rpc/rpc.cc中add_reply实现如下:
    
	void
    rpcs::add_reply(unsigned int clt_nonce, unsigned int xid,
        char *b, int sz)
    {
        ScopedLock rwl(&reply_window_m_);
        std::map<unsigned int, std::list<reply_t> >::iterator clt;
        std::list<reply_t>::iterator iter;
        clt = reply_window_.find(clt_nonce);
        if (clt != reply_window_.end()) {
            for (iter = clt->second.begin(); iter != clt->second.end(); iter++) {
                if (iter->xid == xid) {
                    iter->buf = b;
                    iter->sz = sz;
                    iter->cb_present = true;
                    break;
                }
				}   
			}
        // You fill this in for Lab 1.
	}
在窗口中找到对应的请求(这个请求是在checkduplicate_and_update中加入到窗口的,但是结果还未有效,handler还在处理),然后保存将结果保存.并且置cb_preset为true.表示结果有效. 

最后测试./rpc/rpctest和lock_tester.在网络有丢失的情形下,测试成功.

#Lab 2: Basic File Server
##简介

这次实验主要实现下面的功能

 -  create/mknod, lookup, readdir
 -  setattr, write, read.

 这里包含文件的创建,读写,设置属性,读取目录内容,通过文件名查找
文件的inum等.
整个yfs分布式文件系统的架构如下:

![yfs](https://github.com/ldaochen/yfs2012/raw/lab2/yfs.jpg)

其中yfs客户端(客户端是相对extent server 来说的)负责实现文件逻辑:例如读写操作等.而extent server 负责存储文件数据, 所以extent server的作用就像一个磁盘. 尽管图中有多个yfs运行在不同的主机上,但是它们"看到"的文件系统
都是一样的. 
###第一部分
这一部分需要实现**extent server**和**create/mknod**,**lookup**, **readdir**操作. 先介绍下**extent server**的实现.

在extent_server.h中有extent server的定义. 它的主要是负责存储文件数据,作用类似硬盘. 每个文件都有一个id, 文件内容,和对应的属性三个部分. id类似unix-like系统中的i-node 号
用来唯一标示一个文件. 所以在extent server 中建立起**文件的存储数据结构**如下
   
    pthread_mutex_t mutex;
    struct extent {
        std::string data;
        extent_protocol::attr attr;
    };
    std::map<extent_protocol::extentid_t, extent> file_map;

 - extent中data域是存储文件的数据
 - extent中attr域是存储文件的属性,它包含文件修改时间mtime,文件状态改变时间ctime,文件访问时间atime和文件大小size.
 - file_map 是存储文件的数据结构,每一个文件都是其中的一个<key, value>对.其中key是文件id,value是extent类型的,包含文件内容和属性
 - mutex使得多个yfs客户端可以互斥的访问file_map

extent server 除了存储文件外还提供一些基本的操作
 
- get: 获取文件内容
- put: 将新的文件内容写回extent server.
- getaddr: 获取文件的属性
- remove: 删除一个文件
在建立好文件的存储结构file_map后我们需要实现上面的四个方法.
**get的实现如下**:

        int extent_server::get(extent_protocol::extentid_t id,             std::string &buf)
        {
            // You fill this in for Lab 2.
            ScopedLock _l(&mutex);
            if (file_map.find(id) != file_map.end()) {
                file_map[id].attr.atime = time(NULL);
                buf = file_map[id].data;
                return extent_protocol::OK;
            }
            return extent_protocol::NOENT;
        }
其中**get**获取id对应的文件内容,存放到buf中. 实现很简单,只需要在file_map中查询id对应的文件内容即可,但是需要注意修改文件属性中的atime.

**put的实现如下**:

    int extent_server::put(extent_protocol::extentid_t id,             std::string buf, int &)
    {
        // You fill this in for Lab 2.
        ScopedLock _l(&mutex);
        extent_protocol::attr attr;
        attr.atime = attr.mtime = attr.ctime = time(NULL);
        if (file_map.find(id) != file_map.end())
            attr.atime = file_map[id].attr.atime;
        attr.size = buf.size();
        file_map[id].data = buf;
        file_map[id].attr = attr;
        return extent_protocol::OK;
    }
**put**将buf中的数据写入到id对应的文件中. id可能对应一个新文件,
也可能是一个已经存在的文件.无论是哪种情形都只要将id对应的文件的
内容更新为buf中的内容即可.主要需要修改文件属性ctime和mtime.如果这是一个新文件还需要atime.

**getattr的实现如下**:

    int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
    {
   

        ScopedLock _l(&mutex);
        if (file_map.find(id) != file_map.end()) {
            a = file_map[id].attr;
            return extent_protocol::OK;
        }
        return extent_protocol::NOENT;
    }

**getattr**查找id对应的文件,读取该文件的属性存放到a中.

**remove的实现如下**:

    int extent_server::remove(extent_protocol::extentid_t id, int &)
    {
    // You fill this in for Lab 2.
        std::map<extent_protocol::extentid_t, extent>::iterator iter;
        iter  = file_map.find(id);
        if (iter != file_map.end()) {
            file_map.erase(iter);
            return extent_protocol::OK;
        } else {
            return extent_protocol::NOENT;
        }
    }
**remove**删除id对应的文件.
另外实验要求系统启动后需要有一个名为root的空的根目录文件.所以在extent_server的构造函数中
需要为extent_server创建这样一个root目录文件. 简单的调用**put**即可.实现如下:

    extent_server::extent_server() {
        int ret;
        pthread_mutex_init(&mutex, NULL);
        //root i-number is 1
        put(1, "", ret);
    }


在实现文件的存储extent server后,接下来考虑**create/mknod**, **lookup**和**readdir**的实现.
当用户程序操作**yfs_client**(即前面提到的yfs客户端)管理(served)的文件或者目录(例如测试时产生的yfs1)时, FUSE在内核中的代码会将文件操作请求发送到**yfs_client**, **yfs_client**通过RPC与**extent_server** 交互. 我们需要修改fuse.cc中的代码来调用**yfs_client**中实现的文件逻辑. 在给出具体的实现前给出一些**约定**:

 - 文件id是64位的unsigned long long类型. 其中高32位为0. 低32位表示真实的id. 如果文件id的第31位为0那么这个
id对应的文件是一个目录.为1表示id对应一个文件.
 - 文件目录的格式是一系列<name, inum>键值对.name是文件名,inum是文件id,但是为了方便实现.name前后各加一个"/", inum后加一个"/".所以一个目录项的实际是"/"+name+"/"+inum+"/"字符串.

**create/mknod的实现**:
fuse.cc中fuseserver\_createhelper函数是文件创建的核心函数. 具体的代码实现不列出. 该函数调用**yfs_clent**提供的create方法.
**yfs_clent**中create的实现如下.
        
    int 
    yfs_client::create(inum parent, const char *name, inum &inum)
    {
        int r = OK; 
        std::string dir_data;
        std::string file_name;
        if (ec->get(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }   
        file_name = "/" + std::string(name) + "/";
        if (dir_data.find(file_name) != std::string::npos) {
            return EXIST;
        }   

        inum = random_inum(true);
        if (ec->put(inum, std::string()) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }   

        dir_data.append(file_name + filename(inum) + "/");
        if (ec->put(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
        }   
    release:
        return r;
    }

create函数在目录parent创建一个名为name的文件空文件. 并且为这个文件生成一个唯一id存储在inum中.
首先读取parent的文件内容.查看文件name是否存在. 如果存在返回EXIST. 否则随机生成一个inum,并调用**put**
方法创建一个空文件. 然后将文件名和inum写入parent目录文件中.

**lookup的实现**:
fuse.cc中fuseserver\_lookup函数负责将查询请求发送到**yfs_client**. 也需要我们自己实现,
具体的代码实现不列出. 该函数调用**yfs_clent**提供的lookup方法.
**yfs_clent**中lookup的实现如下.

    int
    yfs_client::lookup(inum parent, const char *name, inum &inum, bool *found)
    {
        int r = OK;
        size_t pos, end;
        std::string dir_data;
        std::string file_name;
        std::string ino;
        if (ec->get(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }
        file_name = "/" + std::string(name) + "/";
        pos = dir_data.find(file_name);
        if (pos != std::string::npos) {
            *found = true;
            pos += file_name.size();
            end = dir_data.find_first_of("/", pos);
            if(end != std::string::npos) {
                ino = dir_data.substr(pos, end-pos);
                inum = n2i(ino.c_str());
            } else {
                r = IOERR;
                goto release;
            }
        } else {
            r = IOERR;
        }
    release:
        return r;
    }
lookup在目录parent查找名为name的文件,如果找到将found设置为true,inum设置为name对应的文件id.
首先读入目录文件parent的内容到dir_data. 然后在其中查找文件名name,注意name前后添加了"/",这是因为前面
我们约定了目录项的格式:"/"+name+"/"+inum+"/", 找到name后,可以根据这三个"/"之间的距离,提取出name和inum.

**readdir的实现**:
fuse.cc中fuseserver\_readdir函数负责将readdir请求发送到**yfs_client**. 也需要我们自己实现,具体的代码实现不列出. 该函数调用**yfs_clent**提供的readdir方法.
**yfs_clent**中readdir的实现如下:

    int 
    yfs_client::readdir(inum inum, std::list<dirent> &dirents) 
    {
        int r = OK; 
        std::string dir_data;
        std::string inum_str;
        size_t pos, name_end, name_len, inum_end, inum_len;
        if (ec->get(inum, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }   
        pos = 0;
        while(pos != dir_data.size()) {
            dirent entry;
            pos = dir_data.find("/", pos);
            if(pos == std::string::npos)
                break;
            name_end = dir_data.find_first_of("/", pos + 1); 
            name_len = name_end - pos - 1;
            entry.name = dir_data.substr(pos + 1, name_len);

            inum_end = dir_data.find_first_of("/", name_end + 1);
            inum_len = inum_end - name_end - 1;
            inum_str = dir_data.substr(name_end + 1, inum_len);
            entry.inum = n2i(inum_str.c_str());
            dirents.push_back(entry);
            pos = inum_end + 1;
        }
    release:
        return r;
    }
readdir读取目录文件inum, 一次提取出<name, inum>的键值对加入到dirents.
首先读取文件inum的内容到dir_data中. 根据目录的格式"/"+name+"/"+inum+"/". 
逐个提取name和inum,最后加入到dirents中. dirents是一个dirent类型的list.
dirent在yfs_client.h中定义如下
    
    struct dirent {
        std::string name;
        yfs_client::inum inum;
    }; 
    
所以提取出的name和inum可以组成一个dirent结构,然后加入到dirents中. 

完成第一部分后使用如下命令进行测试
    
    ./start.sh
    /test-lab-2-a.pl ./yfs1
    ./stop.sh
如果第二条命令结果是Passed all tests!.则实现正确.即使设置RPC_LOSSY为5,也应该是Passed all tests!
## 第二部分.

这部分实现**setattr**, **write**个**read**. 分别是设置文件的属性,读和写文件. 完成这部分后可以实现这样的效果: 一个**yfs_client**写的数据可以被另外一个**yfs_client**读取.

**setattr的实现**:
fuse.cc中fuseserver\_setattr函数函数负责将用户setattr请求发送到**yfs_client**. 也需要我们自己实现, 具体的代码实现不列出. 该函数调用**yfs_clent**提供的setattr方法.
**yfs_clent**中setattr的实现如下:

    int 
    yfs_client::setattr(inum inum, struct stat *attr)
    {
        int r = OK; 
        size_t size = attr->st_size;
        std::string buf;
        if (ec->get(inum, buf) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }   
        buf.resize(size, '\0');

        if (ec->put(inum, buf) != extent_protocol::OK) {
            r = IOERR;
        }   
    release:
        return r;
    }
当前setattr只实现了设置文件大小的功能. 首先读取文件inum的内容,
然后将文件内容调整为size大小. 如果size比文件原来的长度大,那么多出的部分填充'\0'. 最后调用**put**写回文件内容,**put**会根据写回的内容长度修改文件对应的长度属性.

**read的实现**
fuse.cc中fuseserver\_read函数负责将用户read请求发送到**yfs_client**. 也需要我们自己实现, 具体的代码实现不列出. 该函数调用**yfs_clent**提供的read方法.
**yfs_clent**中read的实现如下:

    int 
    yfs_client::read(inum inum, off_t off, size_t size, std::string &buf)
    {   
        int r = OK; 
        std::string file_data;
        size_t read_size;
        
        if (ec->get(inum, file_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }   
        
        if (off >= file_data.size())
        buf = std::string();
        read_size = size;
        if(off + size > file_data.size()) 
            read_size = file_data.size() - off;
        buf = file_data.substr(off, read_size);
    release:
        return r;   
    }

read函数从文件inum的偏移off处读取size大小的字节到buf中. 如果off比文件长度大则读取0个字节.
如果off+size 超出了文件大小. 则读到文件尾即可. 该函数首先调用**get**读取文件inum的内容到
file_data中.然后根据off和size的大小,从file_data中取出合适的字节数到buf.

**write的实现**:
fuse.cc中fuseserver\_write函数负责将用户write请求发送到**yfs_client**. 也需要我们自己实现,具体的代码实现不列出. 该函数调用**yfs_clent**提供的write方法.
**yfs_clent**中write的实现如下:

    int 
    yfs_client::write(inum inum, off_t off, size_t size, const char *buf)
    {
        int r = OK; 
        std::string file_data;
        if (ec->get(inum, file_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }
        if (size + off > file_data.size())
            file_data.resize(size + off, '\0');
        for (int i = 0; i < size; i++)
            file_data[off + i] = buf[i];
        if (ec->put(inum, file_data) != extent_protocol::OK) {
            r = IOERR;
        }
    release:
        return r;
}

write函数将buf中size个字节写入到文件inum的偏移off处. 这里需要考虑几种情形

 1.  off比文件长度大, 那么需要将文件增大,并且使用'\0'填充,然后再从off处写入.
 2.  off+size比文件长度大, 那么文件的大小将会根据写入的数据大小也会相应的调整.

write首先读取文件inum的内容到缓冲区file_data. 接下来调整file_data的大小为最终的大小,
即off+size, 然后往file_data中写入数据. 最后调用**put**方法写回file_data中的数据.

#Lab 3: MKDIR, UNLINK, and Locking
##简介
这次实现包含两个部分.

 1. 实现mkdir和unlink操作.
 2. 增加锁确保并发的文件访问执行正确.
 
整个yfs分布式文件系统的架构如下:

![yfs](https://github.com/ldaochen/yfs2012/raw/lab2/yfs.jpg)

其中yfs客户端(客户端是相对extent server 来说的)负责实现文件逻辑:例如读写操作等.而extent server 负责存储文件数据, 所以extent server的作用就像一个磁盘.尽管图中有多个yfs运行在不同的主机上,但是它们"看到"的文件系统.

###第一部分
这部分主要实现mkdir和unlink操作.

**mkdir的实现**:
mkdir的实现和lab2中的create的实现很类似. mkdir是在父目录中创建一个新的空目录.按照lab2中的介绍,目录文件的
inum的第31位是0. 在fuse.cc中fuseserver_mkdir函数将用户的mkdir的操作请求发送给**yfs_client**. 主要的mkdir实现就是yfs_client->mkdir(...)方法.其实现如下

    int
    yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &inum)
    {
        int r = OK;
        std::string dir_data;
        std::string dirent, dirname;
        ScopedLockClient mlc(lc, parent);
        if (ec->get(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }
        dirname = "/" + std::string(name) + "/";
        if (dir_data.find(dirname) != std::string::npos) {
            return EXIST;
        }
        inum = random_inum(false);

        if (ec->put(inum, std::string()) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }

        dirent = dirname + filename(inum) + "/";
        dir_data += dirent;

        if (ec->put(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
        }

    release:
        return r;
    }

mkdir的实现逻辑和lab2中的create实现逻辑基本一样,不同的是mkdir创建的是一个新的空目录,create创建的是一个新普通文件. 目录对应的inum第31为0,文件对应的inum第31位为1.所以这里random_inum的参数是false,表示是一个目录,返回的inum的第31为0.

**unlink的实现**
unlink用于删除一个文件.fuse.cc中的fuseserver_unlink函数负责将用户的unlink请求发送给**yfs_client**.yfs_client->unlink实现了主要的删除逻辑. 实现如下

    int
    yfs_client::unlink(inum parent, const char *name)
    {
        int r = OK;
        std::string dir_data;
        std::string filename = "/" + std::string(name) + "/";
        size_t pos, end, len;
        inum inum;
        ScopedLockClient ulc(lc, parent);
        if (ec->get(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }
        if ((pos = dir_data.find(filename)) == std::string::npos) {
            r = NOENT;
            goto release;
        }

        end = dir_data.find_first_of("/", pos + filename.size());
        if( end == std::string::npos) {
            r= NOENT;
            goto release;
        }
        len = end - filename.size() - pos;
        inum = n2i(dir_data.substr(pos+filename.size(), len));
        if (!isfile(inum)) {
            r = IOERR;
            goto release;
        }
        dir_data.erase(pos, end - pos + 1);
        if (ec->put(parent, dir_data) != extent_protocol::OK) {
            r = IOERR;
            goto release;
        }
        if (ec->remove(inum) != extent_protocol::OK) {
            r = IOERR;
        }
    release:
        return r;
    }
    
unlink从目录parent中删除名为name的文件. 首先读取目录文件parent的内容到dir_data中. 如果name在目录parent中不存在或者name对应的是一个目录,那么返回出错信息. 如果找到了name,提取出名为name的文件对应的inum. 调用**extent_client**提供的remove方法删除该文件.
然后在目录parent中删除name对应的目录项.

###第二部分

这部分主要处理文件系统的并发访问带来的问题. 例如**yfs_client**的**create**操作, 这个操作需要从extent server中读取目录的内容, 做一些修改,然后将修改后的内容写回extent server. 假设两个**yfs_client**同时在相同的目录中创建名字不同的文件. 那么两个**yfs_client**可能都会从extent server中获取旧的内容.然后写入新的目录项,最后两个**yfs_clent**将修改后的内容写回到exten_server. 那么结果是只有一个**yfs_client**写的目录项会保存,另一个被覆盖了. 正确的结果应该是这两个目录项都存在. 类似的存在还存在create和unlink之间, mkdir和unlink, write和write等. 

为了保证并发访问的正确,我们为每个文件设置一个锁, 并且使用文件的inum作为锁的id. 因为锁是在单独一个服务上. 所以**yfs_client** 需要维护一个锁客户端. 代码如下

    class yfs_client {
        extent_client *ec;
        lock_client *lc;
        public:
        ...
    }
这里在**yfs_client** 中添加了一个**lock_client**,即lc.
同时新增了一个类ScopeLockClient.

    class ScopedLockClient {
        private:
            lock_client *lc_;
            lock_protocol::lockid_t lid;
        public:
            ScopedLockClient(lock_client *lc, lock_protocol::lockid_t lid):
                lc_(lc),lid(lid) {
                lc_->acquire(lid);
            }
            ~ScopedLockClient() {
                lc_->release(lid);
            }
    };
    
当实例化一个ScopedLocKClient对象是会自动获取锁, 当这个对象析构时自动的释放锁. 所以不需要显示的获取锁和释放锁.

根据前面的分析,**yfs_client**中需要加锁的函数包括
**create**, **unlink**, **mdkir**, **write**和**setattr**. 只需要在这些函数中实例化一个
ScopedLockClient对象即可. 具体代码可以查看第一部分中给出的代码.


#Lab 4: Caching locks
##简介
这次实验实现在锁客户端缓存锁,减轻锁服务器的负载. 例如应用使用YFS在同一目录下连续创建100个文件,按照LAB 3
的实现,需要acquire 100次目录锁,同样也需要release 100次. 这种方式增加了锁服务器的负载,其实只需要acquire一次锁,然后缓存锁,最后使用完后再释放就可以了,这样之前的100次acquire和100次release只需要1次acquire和1次release. 本次实验实现acquire锁后在锁客户端缓存锁, 当有其他客户端需要该锁时再释放该锁.

##设计要求

下面是实现要求,必须要满足这些要求

 1. 每个客户端可能有多个线程获取同一个锁,但是每一个客户端只允许一个线程(并不要求是哪个特定的线程,而是不允许多个线程与锁服务器交互)与锁服务器进行交互, 一旦一个线程已经获取到锁,然后释放锁就可以唤醒同一客户端在等待的其他线程
 2. 当一个客户端使用**acquire** RPC请求一个锁,如果锁没有被其他的客户端获取(FREE),那么锁服务器返回OK, 如果锁不是FREE,并且有其他的客户端等待这个锁,那么返回一个ENTRY. 如果锁不是FREE,并且没有其他客户端等待这个锁,那么锁服务器**revoke** RPC到锁的拥有者,等待锁的拥有者释放这个锁,最终锁服务器发送一个**retry** RPC给等待该锁的下一个客户端,通知它再次尝试获取.
 3. 一旦一个客户端拥有了一个锁,那么客户端缓存这个锁(即当释放这个锁时并不发送一个**release** RPC给锁服务器). 客户端可以将锁给同一个客户端的其他线程而且不需要与服务器交互.
 4. 锁服务器通过发送一个**revoke** RPC给客户端来收回客户端缓存的锁,这个**revoke**请求告诉客户端当**release**锁时立即将锁返回给服务器,或者如果没有线程当前持有这个锁,那么就立即将锁返回给锁服务器.
 5. 锁服务器应该记录每个锁的持有者ID(hostname:port),这样锁服务器才知道**revoke** RPC发送给哪个客户端. 另外还需要记录哪些客户端在等待这个锁,这样锁服务器可以向其中一个发送**retry** RPC. 
 6. 当发送一个RPC时不要持有任何mutex, RPC通常需要较长的时间,这会让其他的线程一直等待,另为在RPC时持有mutex容易导致分布式锁死.
 
除了上述的要求外还需要注意下面的问题
 
 1. 当客户端的一个线程持有一个锁,同一客户端的另一个线程**acquire**尝试获取锁时不需要给服务器发送RPC
 2. 一个线程在持有一个锁时收到**revoke**消息如何处理,另外线程在**acquire**时先收到**retry** RPC,再收到**ENTRY**返回值怎么处理.
 3. 当一个线程在尝试**acquire**,在收到**acquire**的返回值前收到**revoke** RPC如何处理

 
##协议设计
###锁客户端协议处理
锁客户端中缓存的锁有下面5个状态

 - **NONE**: 客户端不知道该锁的任何信息,该锁可能还在服务器上,或者被别的客户端持有.
 - **FREE**:当前客户端拥有这个锁,并且在这个客户端上没有线程持有这个锁.
 - **LOCKED**: 当前客户端拥有这个锁,并且锁被某个线程持有.
 - **ACQUIRGING**: 当前客户端有线程正在向服务器尝试获取这个锁.
 - **RELEASING** 当前客户端正在尝试将锁返回给服务器.
 
同时每个锁维护3个条件变量.
 - **retryqueue**:初始时锁的状态为**NONE**,当客户端线程尝试向服务器获取锁时,将锁的状态修改为**ACQURING**. 同时线程可能收到来自服务器的OK或者RETRY. 当收到RETRY时将线程挂起到**retryqueue**条件变量上. 
 - **waitqueue**: 当客户端线程尝试向获取锁时,发现锁的状态是**ACQURING**,说明之前已经有线程已经向服务器请求拥有锁, 所以不需要再和服务器进行交互(每个客户端每次只有一个线程与服务器交互). 将自身挂起到**waitqueue**队列上.
 - **releasequeue**: 当客户端线程收到**revoke** RPC,在释放锁时需要向锁服务器发送**release** RPC返回锁给服务器.这时锁的状态修改为**RELEASEING**, 如果此时客户端有线程**acquire**这个锁,那么需要将这个线程挂起到**releasequeue**队列上. 当**release** RPC
完成后将锁的状态修改为NONE,同时唤醒**releasequeue**上的线程.
另为客户端每个锁还维护两个布尔变量.
 - **revoked**: false表示未收到 **revoke** RPC, true表示已收到. 如果没有收到那么在执行**release** 操作时只需要将锁的状态从**LOCKED**修改为**FREE**即可,然后唤醒**waitqueue**上的另一个线程,不需要和锁服务器交互. 如果已经收到**revoke**, 那么在需要将锁返回给服务器. 具体是先将状态修改为**RELEASING**,然后发送**release**给服务器,完成后修改锁的状态为NONE. 然后唤醒**releasequeue**上的线程
 - **retry**: false表示未收到**retry** RPC. true表示已经收到. 当收到**retry**后唤醒**retryqueue**上等待的线程. 此时也有可能**retryqueue**还未空(见前面提到的第2个问题),
 就是还未收到**ENTRY**,还没来得及将自己加入到**retryqueue**中,这时就收到了**retry**了.既然收到了**retry**
就没必要将自己加入到**retryqueue**,再次尝试向服务器端获取锁就行了. 所以在加入到**retryqueue**前需要判断
是否收到**retry**.

前面提到的**第一个问题**时通过**ACQUIRGING**状态解决的,只要是这个状态表明有线程和服务器交互了,其他的线程不需要和服务器交互. **第三个问题**的处理是:只有在释放锁时才检测**revoked**的值, 即使在**acquire**阶段就收到了**revoke** RPC.也要等到这个**acquire**对应的**release**调用时才处理.

###锁服务器端协议处理.
锁服务器端的锁有下面四个状态

 - **FREE**: 该锁未被任何客户端持有
 - **LOCKED**: 锁被某个客户端持有,没有其他客户端等待该锁
 - **LOCKED_AND_WAIT**: 锁被某个客户端持有,并且有其他客户端等待该锁
 - **RETRYING**: 锁服务器正在向某个客户端发送**retry** RPC.

同时服务器端每个锁还有下面两个成员

 - **owner**: 如果锁被某个客户端持有,owner记录持有者的地址和端口号(hostname:port),便于给它发送**revoke** RPC
 - **waitset**:记录等待该所的所有客户端,其中存储这些客户端地址和端口号(hostname:port).便于发送**retry** RPC
 
**当服务器收到一个客户端发来的acquire  RPC时**

如果对应的锁的状态时**FREE**,直接将锁的状态修改为为LOCED,同时在**owner**中记录这个客户端地址和端口号.
然会OK. 
如果锁的状态是**LOCKED**,则将锁的状态修改为**LOCKED_AND_WAIT**,将该客户端地址和端口号保存到**waitset**然后发送返回一个**ENTRY**给客户端.并且发送一个**revoke** RPC给锁的持有者. 
如果锁的状态是**LOCKED_AND_WAIT**,保持**LOCKED_AND_WAIT**状态不变.将该客户端地址和端口号保存到**waitset**然后发送返回一个**ENTRY**给客户端,不需要发送**revoke** RPC.
如果锁的状态是**RETRYING**,表示锁服务器正在发送**retry**给客户端(计为A),那么此时的**acquire**可能是客户端A
收到的**retry**后发送的一个**acquire** RPC. 这时查看**waitset**中是否包含客户端A.如果包含说明这就是客户端A发送来的. 将客户端A从**waitset**中删除. 如果**waitset**中没有其他的客户端等待,将锁的状态修改为**LOCKED**,将**owner**修改为客户端A的地址和端口.返回OK,如果**waitset**中还有其他客户端等待.那么将锁的状态修改为**LOCKED_AND_WAIT**. 给客户端返回OK同时发送**revoke**,因为还有其他客户端等待所以需要发送**revoke**. 如果这个**acquire**不是客户端A发送的. 那么将这个客户端的地址和端口号保存在**waitset**中,返回一个**RETRY**就可以了. 


**当服务器端收到一个release RPC时**

如果锁的状态是**FREE**或者**RETRYING**表示出错,返回一个出错信息给客户端.
如果锁的状态时**LOCKED**, 将锁状态修改为**FREE**,并将**owner**清空.
如果锁的状态是**LOCKED_AND_WAIT**,则将锁的状态修改为**RETRYING**,将**owner**清空. 向**waitset**中的一个客户端发送**retry** RPC.


##代码实现
主要的代码实现在lock_client_cache.h/lock_client_cache.cc,lock_server_cache.h/lock_server_cache.cc和
lock_smain.cc文件中.

#Lab 5: Caching Extents
##简介
这次实验实在文件内容客户端(extent_client)实现文件内容和文件属性的缓存,减轻文件内容服务(extent_server)的负载.文件相应的操作都时在文件缓存中进行，仅仅当缓存中不存在相应的文件内容或者属性时才需要访问文件内容服务(extent_server). 这次实验主要需要考虑两个问题:

 1. 如果设计文件缓存相应的数据结构
 2. 如果保证一致性

##文件缓存
文件内容客户端(extent_client)定义一个子类

    class extent_client_cache : public extent_client {
	    enum file_state {NONE,UPDATED, MODIFIED, REMOVED};
	    struct extent {
	    	std::string data;
	    	file_state status;
	    	extent_protocol::attr attr;
	    	extent():status(NONE) {}
	    };
	    public:
		    extent_client_cache(std::string dst);
		    extent_protocol::status get(extent_protocol::extentid_t eid,
			      std::string &buf);
		    extent_protocol::status getattr(extent_protocol::extentid_t eid, extent_protocol::attr &a);
		    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
		    extent_protocol::status remove(extent_protocol::extentid_t eid);
		    extent_protocol::status flush(extent_protocol::extentid_t eid);
	    private:
		    pthread_mutex_t extent_mutex; 
		    std::map <extent_protocol::extentid_t, extent>file_cached;
    }

其中get，getaddr,put,remove操作都将进行重载，这些函数首先都访问文件缓存(file_cacahed),如果文件缓存中不存在相应的内容才会访问文件内容服务(extent_server). 这个新的文件内容客户端类中文件缓存的定义如下:
        
        pthread_mutex_t extent_mutex; 
	    std::map <extent_protocol::extentid_t, extent>file_cached;
     
extent_mutex用户多线程互斥的访问file_cacahed. file_cached中key时文件id.
value时extent.extent的定义如下:

        struct extent {
	        std::string data;
	        file_state status;
	    	extent_protocol::attr attr;
	    	extent():status(NONE) {}
	    };

 - data:表示文件的数据.
 - attr:表示文件的属性
 - status: 表示这个文件在缓存中的状态.这个状态有四种:

        enum file_state {NONE,UPDATED, MODIFIED, REMOVED};
 

 - NONE:表示文件内容不存在，只缓存了文件属性.
 - UPDATED:表示缓存了文件内容.并且文件内容没有被修改.此时文件内容可能已缓存也可能未缓存.
 - MODIFIED:表示缓存了文件内容.并且内容已经被修改.文件属性可能已缓存,也可能未缓存.
 - REMOVED:表示文件已删除.

get，getaddr,put,remove的实现可以查阅extent_client_cache.cc.

##缓存一致性
因为在文件的读写都在文件缓存中进行,为了保证一致性(即读操作获取的内容必须是最近的写操作写的内容). yfs采用**释放一致性**来保证一致性. 因为yfs中文件的id(i-number号)和锁id时同样的值.当释放一个锁回锁服务器时，必须确保文件内容客户端(extent_client)中对应的缓存文件也flush回了文件内容服务(extent_server).并且从缓存中删除这个文件. flush操作检查文件内容是否已经修改，如果是则讲新的内容put到文件内容服务.如果文件被删除(即状态REMOVED),那么从文件内容服务上删除这个文件.

例如客户端A获取一个文件的锁，然后从文件内容服务get文件的内容.并在本地缓存中修改这个文件的内容. 此时客户端B也尝试获取这个文件的锁.这时锁服务器给客户端A发送revoke消息.然后客户端A在将锁释放回锁服务器前先把已修改的文件内容flush回文件内容服务. 然后客户端B会获取到这个锁.在从文件内容服务get文件内容(B的缓存中不会已经缓存了这个文件,所以必须访问文件内容服务.如果曾经缓存了，在释放锁时也将这个项缓存删除了).这时客户端B获取到的内容就是A修改后的内容.

首先我们需要实现flush操作.在extent_client_cache类中定义了成员函数flush.其实现在extent_client_cache.cc中.


    extent_protocol::status
    extent_client_cache::flush(extent_protocol::extentid_t eid)
    {
	    extent_protocol::status ret = extent_protocol::OK;
	    int r;
	    ScopedLock _m(&extent_mutex);
	    bool flag = file_cached.count(eid);
	    if (flag) {	
		    switch(file_cached[eid].status) {
			    case MODIFIED:
				    ret = cl->call(extent_protocol::put, eid,                               file_cached[eid].data, r);
				    break;
			    case REMOVED:
				    ret = cl->call(extent_protocol::remove, eid);
				    break;
			    case NONE:
			    case UPDATED:
			    default:
				break;
		    }
		    file_cached.erase(eid);	
	    } else {
		    ret = extent_protocol::NOENT;
	    }
	    return ret;
    }

从中看到如果文件已经修改则需要put回文件内容服务.如果是已删除则需要在文件内容服务上也删除这个文件.无论缓存中eid文件的状态怎么样.最后都需要从本地缓存中删除:

    file_cached.erase(eid);	    

我们没有必要显示的将缓存的文件属性写回文件内容服务.当flush将已修改的内容
写回到文件内容服务时.文件内容服务会自动更新文件的属性.

###flush的调用点
前面提到只有当锁被释放回锁服务器时才会讲文件缓存内容更新到文件内容服务.
所以flush应该实在release中被调用.lock_client_cache类release函数中必须在释放锁到锁服务器前调用它的lu成员的dorelease函数. lu是个lock_release_user类的对象,dorelease是它的一个虚函数，然后在yfs_client.h中定义该类的子类，并重载dorelease. 

    class lock_user : public lock_release_user {
	    public:
		    lock_user(extent_client_cache *e) : ec(e) {}; 
		    void dorelease(lock_protocol::lockid_t lid) {
			    ec->flush(lid);
		    }
        private:
            extent_client_cache *ec;
    };
    

可以看到dorelease直接调用flush操作. 最后我们将dorelease函数插入到锁客户端(lock_client_cache)中释放回锁服务器的地方:
    

 1. lock_client_cache::release(lock_protocol::lockid_t lid)
 2. lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &)

这两个函数中都有释放锁回锁服务器的操作,所以插入了dorelease.

因为现在锁的作用不单是保证文件系统操作的原子性，而且还驱动文件缓存更新到文件内容服务来保证一致性.所以之前的实验中不需要加锁的部分也需要加锁.例如read操作.并且确保yfs_client中的每一个get(eid),put(eid),getattr(eid)和
remove(eid)调用前后被acquire(eid)/release(eid)所包围.

###存在的问题
文件属性的更新只有在flush调用put时才会更新到文件内容服务，但是如果一个文件只是被读取,并在客户端缓存,虽然文件内容没变,但是缓存的文件属性中的atime却更新了.但是flush操作并不会将这个未修改的文件写回到文件服务.所以文件内容服务上对应文件的atime却没有更新.即读操作没有更新atime.导致另一个客户端调用getattr时得到的atime不是正确的.

#Lab 6: Paxos
##简介
之前的实现中没有考虑**锁服务器**会failure的情形. 考虑到这种情形我们采用**replicated state machine(RSM)**方法来备份锁服务器.
###RSM
RSM基本的想法是这些机器初始状态相同,那么执行相同的操作系列后状态也是相同的. 因为网络乱序等原因,无法保证所有备份机器收到的操作请求序列都是相同的.所以采用一机器为master,master从客户端接受请求,决定请求次序,然后发送给各个备份机器.然后以相同的次序在所有备份(replicas)机器上执行,master等待所有备份机器返回,然后master返回给客户端.当master失败. 任何一个备份(replicas)可以接管工作.因为他们都有相同的状态.
###Paxos
上面的RSM的核心是要所有机器达成一个协议:哪一个备份(replica)是master,而哪些slave机器是正在运行的(alive),并没有fail.因为任何机器在任何时刻都有可能失败.

**Paxos**算法可以参考 [Paxos Made Simple](http://pdos.csail.mit.edu/6.824-2012/papers/paxos-simple.pdf)这篇文章. 这里不做多介绍,主要说明下各种情形下的问题，实验中**Paxos**的伪代码实现如下:

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
 
 详细的代码实现见Paxos.cc
 
###关于Paxos的问题
 自己思考的几个问题:

 1. Paxos基于达成一致需要什么前提?
 少于N/2个节点失败, 不存在拜占庭错误(non-Byzantine model)
 2. Paxos为什么要两阶段,一阶段不行吗?
 课件中指出:"Paxos has two phases
 proposer can't just send "do you promise to commit to this value?" can't promise: maybe everyone else just promised to a different value have to be able to change mind so: prepare, and accept", 意思就是每个阶段不知道其他节点的信息，所以不能在一个阶段中就做出promise，可能其他阶段做了不同的promise,所以需要两个阶段.
 3. 为什么每个proposals需要一个编号?
论文[Paxos Made Simple](http://pdos.csail.mit.edu/6.824-2012/papers/paxos-simple.pdf)给出的解释如下:"Several values could be proposed by different proposers at about the same time, leading to a situation in which every acceptor has accepted a value, but no single value is accepted by a majority of them. Even with just two proposed values, if each is accepted by about half the acceptors, failure of a single acceptor could make it impossible to learn which of the values was chosen.
P1 and the requirement that a value is chosen only when it is accepted by a majority of acceptors imply that an acceptor must be allowed to accept
more than one proposal. We keep track of the different proposals that an acceptor may accept by assigning a (natural) number to each proposal, so a proposal consists of a proposal number and a value. To prevent confusion, we require that different proposals have different numbers." 即如果不编号,可能会导致每个acceptor都接受一个议案，形成不了大多数
 4. 为什么需要leader?
 论文[Paxos Made Simple](http://pdos.csail.mit.edu/6.824-2012/papers/paxos-simple.pdf)给出的解释如下:"
The algorithm chooses a leader, which plays the roles of the distinguished proposer and the distinguished learner",其中 "distinguished proposer"是为了解决活锁问题. "distinguished learner"
是为了介绍通信复杂度(详见论文section2.3).
 5. What if leader fails while sending accept?
 6. What if a node fails after receiving accept?
    + If it doesn’t restart …
    + If it reboots …
 7. What if a node fails after sending prepare-ok?
    + If it reboots …
 8. What if there is a network partition?
 课件中给出的解释是"
What would happen if network partition(总共3个server s1 s2 s3)?
  I.e. S3 was alive?
  S3 would also initiate Paxos for new view
  S3's prepare would not assemble a majority"
 9. What if a leader crashes in the middle of solicitation?
 10. What if a leader crashes after deciding but before
announcing results?
 11. what if an acceptor crashes after receiving accept?
A1: p1  a1v1
A2: p1  a1v1 reboot  p2  a2v?
A3: p1               p2  a2v?
A2 must remember v_a/n_a across reboot! on disk
  might be only intersection with new proposer's majority
  and thus only evidence that already agreed on v1
 12. what if an acceptor reboots after sending prepare_ok?
 does it have to remember n_p on disk?
 if n_p not remembered, this could happen:
 S1: p10            a10v10
 S2: p10 p11 reboot a10v10 a11v11
 S3:     p11               a11v11
 11's proposer did not see value 10, so 11 proposed its own value
 but just before that, 10 had been chosen!
 b/c S2 did not remember to ignore a10v10
 13. can Paxos get stuck?
  yes, if there is not a majority that can communicate
  how about if a majority is available?
 14. leader选举算法,怎么选出leader?
 课件中采用的最小id的作为leader,原文:"always have the primary be the live server with lowest ID".
 15. leader宕机,但新的leader还未选出,对系统会有什么影响?

 #Lab 7: Replicated State Machine
 ##简介
 本次需要实现**锁服务**的fault tolerance. 具体时现在采用RSM(Replicated State Machine)来实现. 
 要找工作没时间写文档了～有时间在写吧. Paxos和RSM真是...难写难调啊。
 