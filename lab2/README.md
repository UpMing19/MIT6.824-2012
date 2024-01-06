# Lab2 : Basic File Server

## 主要实现create、lookup、readdir、write、read、setattr函数

## 主要流程：
    新增yfs- client、FUSE、extent-client、extent-server

    用户调用的FUSE的接口（例如Create，LookUp等等），FUSE接口中的具体实现是调用的yfs-client当中相应函数

    然后通过发送rpc请求调用的extent-server中的具体方法，但是与yfs-client直接通信的是在extent-server之前的extent-client（由他发送rpc请求）

## 主要方法
### extent-server
    主要是KV存储结构，完善put,get,remover,getattr方法
    这里的K是inum一个唯一的序列号表示文件路径
    V是文件内容，其中包含 
    struct attr {
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;//文件大小
    };
    和 string 
### yfs-client
    yfs_client::inum yfs_client::random_inum(bool isfile) 返回一个随机的inum编号，参数决定第31位的0/1值，代表文件或者文件夹

    int yfs_client::create(inum parent, const char *name, inum &inum) 参数分别为夫路径，文件名，文件唯一inum
    首先查询路径下有没有同名文件，然后随机一个inum，再用put方法给文件put一个空字符串代表创建
    然后将文件名追加到父目录下
     约定：filename =  ‘/’ + name + ’/‘ + “inum” + “/”

     int yfs_client::lookup(inum parent, const char *name, inum &inum, bool *found)
    还是先get出父目录下的内容，然后find，根据string中的api定位到inum（字符串切割）

    int yfs_client::readdir(inum inum, std::list<dirent> &dirents)
    同lookup，也是字符串切割获取多个<name + inum>的复合

    int yfs_client::write(inum inum, off_t off, size_t size, const char *buf)
    如果off+size超过filesize需要扩容然后从off出写size个字节进
    int yfs_client::read(inum inum, off_t off, size_t size, std::string &buf)
    使用循环读即可，注意不足size的时候返回剩余的可用字节
    int yfs_client::setattr(inum inum, struct stat *attr)
    get出来后设置文件大小然后resize重新put即可



## 测试
    ./start.sh 启动yfs-client1和yfs-client2并且将同一个文件系统挂载到 /yfs1 和/yfs2
    ./test-lab-2-b.pl ./yfs1 ./yfs2 测试
    RPC_LOSSY=5同样要通过测试




