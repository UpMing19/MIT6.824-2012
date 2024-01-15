// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

#include "rsm_client.h"
#include "rpc/fifo.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;

  virtual ~lock_release_user(){};
};

class lock_client_cache_rsm;

// Clients that caches locks.  The server can revoke locks using
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
  enum lock_status { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

  struct lock_entry {
    lock_protocol::xid_t xid = 0;  // sequence number
    lock_status status = NONE;
    bool revoke = false;
    bool retry = false;
  };

  struct release_entry {
    lock_protocol::lockid_t lid = 0;
    lock_protocol::xid_t xid = 0;  // sequence number
    release_entry(lock_protocol::lockid_t l = 0, lock_protocol::xid_t x = 0)
        : lid(l), xid(x){};
  };
  fifo<release_entry> releaseFifo;

  std::map<lock_protocol::lockid_t, lock_entry> m_lockMap;
  std::mutex m_mutex;
  std::condition_variable waitQueue;
  std::condition_variable retryQueue;
  std::condition_variable releaseQueue;

 private:
  rsm_client *rsmc;

  class lock_release_user *lu;

  int rlock_port;
  std::string hostname;
  std::string id;
  lock_protocol::xid_t xid;

 public:
  static int last_port;

  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);

  virtual ~lock_client_cache_rsm(){};

  lock_protocol::status acquire(lock_protocol::lockid_t);

  virtual lock_protocol::status release(lock_protocol::lockid_t);

  void releaser();

  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                        lock_protocol::xid_t, int &);

  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                       lock_protocol::xid_t, int &);
};

#endif
