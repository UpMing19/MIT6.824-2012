#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <map>
#include <set>
#include <string>

#include "lock_protocol.h"
#include "lock_server.h"
#include "rpc.h"

class lock_server_cache {
 private:
  int nacquire;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);

 private:
  enum lock_state {
    FREE,
    LOCKED,
    LOCKED_AND_WAIT,
    RETRYING,
  };
  struct lock_entry {
    std::string owner;
    std::set<std::string> waitSet;
    bool revoked;
    lock_state state;
    lock_entry() : state(FREE), revoked(false){};
  };
  std::mutex m_mutex;
  std::map<lock_protocol::lockid_t, lock_entry> m_lockMap;
};

#endif
