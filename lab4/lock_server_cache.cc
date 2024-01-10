// the caching lock server implementation

#include "lock_server_cache.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <sstream>

#include "handle.h"
#include "lang/verify.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache() {}
lock_protocol::status lock_server_cache::stat(lock_protocol::lockid_t lid,
                                              int &r) {
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &) {
  std::unique_lock<std::mutex> lck(m_mutex);
  lock_protocol::status ret = lock_protocol::OK;

  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) {
    it = m_lockMap.insert(std::make_pair(lid, lock_entry())).first;
  }
  bool revoke = false;
  if (it->second.state == FREE) {
    it->second.state = LOCKED;
    it->second.owner = id;
  } else if (it->second.state == LOCKED) {
    revoke = true;
    it->second.waitSet.push(id);
    it->second.state = LOCKED_AND_WAIT;
    ret = lock_protocol::RETRY;
  } else if (it->second.state == LOCKED_AND_WAIT) {
    revoke = true;
    it->second.waitSet.push(id);
    ret = lock_protocol::RETRY;
  } else if (it->second.state == RETRYING) {
    if (it->second.waitSet.front() == id) {
      it->second.waitSet.pop();
      it->second.owner = id;
      if (it->second.waitSet.size()) {
        it->second.state = LOCKED_AND_WAIT;
        revoke = true;
      } else {
        it->second.state = LOCKED;
      }
    } else {
      it->second.waitSet.push(id);
      ret = lock_protocol::RETRY;
    }
  }
  lck.unlock();
  if (revoke) {
    int r;
    handle(it->second.owner).safebind()->call(rlock_protocol::revoke, lid, r);
  }
  return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
                               int &r) {
  std::unique_lock<std::mutex> lck(m_mutex);
  int ret = lock_protocol::OK;
  bool retry = false;
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) {
    lck.unlock();
    return lock_protocol::IOERR;
  }
  std::string client_id;
  if (it->second.state == FREE || it->second.state == RETRYING) {
    lck.unlock();
    return lock_protocol::IOERR;
  }

  if (it->second.state == LOCKED) {
    it->second.state = FREE;
    it->second.owner = "";
  } else {
    it->second.state = RETRYING;
    it->second.owner = "";
    client_id = (it->second.waitSet.front());
    retry = true;
  }

  lck.unlock();
  if (retry) {
    int r;
    handle(client_id).safebind()->call(rlock_protocol::retry, lid, r);
  }
  return ret;
}
