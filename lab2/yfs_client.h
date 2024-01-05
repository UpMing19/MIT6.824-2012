#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"

class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);


};

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
class lock_user : public lock_release_user {
	public:
		lock_user(extent_client_cache *e) : ec(e) {}; 
		void dorelease(lock_protocol::lockid_t lid) {
			ec->flush(lid);
		}
    private:
       extent_client_cache *ec;
};
#endif 
