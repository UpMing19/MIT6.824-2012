// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <map>
#include <string>

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

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache(){};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);

 private:
  enum lock_state { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

  struct lock_entry {
    lock_state state;
    bool revoke, retry;

    pthread_cond_t waitQueue;
    pthread_cond_t releaseQueue;
    pthread_cond_t acquireQueue;

    lock_entry() : state(NONE), revoke(false), retry(false) {
      pthread_cond_init(&waitQueue, NULL);
      pthread_cond_init(&releaseQueue, NULL);
      pthread_cond_init(&acquireQueue, NULL);
    };
  };
  std::map<lock_protocol::lockid_t, lock_entry> m_lockMap;
  pthread_mutex_t m_mutex;
};

#endif
