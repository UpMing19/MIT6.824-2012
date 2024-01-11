// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "extent_client_cache.h"
#include "lang/verify.h"
#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user(){};
};
class lock_user : public lock_release_user {
  extent_client_cache *ec;

 public:
  lock_user(extent_client_cache *e) : ec(e){};
  void dorelease(lock_protocol::lockid_t lid) { ec->flush(lid); };
  ~lock_user(){};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache(){};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);

 private:
  enum lock_state { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

  struct lock_entry {
    // 记录是否收到revokedRPC
    bool revoked;
    // 记录是否收到retryRPC
    bool retry;
    lock_state state;

    lock_entry() : revoked(false), retry(false), state(NONE) {}
  };

  std::map<lock_protocol::lockid_t, lock_entry> m_lockMap;
  std::mutex m_mutex;

  // 三个条件变量
  std::condition_variable waitQueue;
  std::condition_variable releaseQueue;
  std::condition_variable retryQueue;
};

#endif
