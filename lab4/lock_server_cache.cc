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
  bool revoke = false;
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) {
    lock_entry lock;
    it = m_lockMap.insert(std::make_pair(lid, lock)).first;
  }

  switch (it->second.state) {
    case FREE:
      it->second.owner = id;
      it->second.state = LOCKED;
      break;
    case LOCKED:
      it->second.state = LOCKED_AND_WAIT;
      it->second.waitSet.insert(id);
      ret = lock_protocol::RETRY;
      revoke = true;
      break;
    case LOCKED_AND_WAIT:
      it->second.waitSet.insert(id);
      ret = lock_protocol::RETRY;
      break;
    case RETRYING:
      if (it->second.waitSet.count(id)) {
        it->second.waitSet.erase(id);
        it->second.owner = id;
        if (it->second.waitSet.size()) {
          it->second.state = LOCKED_AND_WAIT;
          it->second.revoked = true;
        } else {
          it->second.state = LOCKED;
        }
      } else {
        it->second.waitSet.insert(id);
        ret = ret = lock_protocol::RETRY;
      }
      break;

    default:
      break;
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

  lock_protocol::status ret = lock_protocol::OK;
  bool retry = false;
  std::string client_need_retry = "";

  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) {
    ret = lock_protocol::NOENT;
  } else {
    switch (it->second.state) {
      case FREE:
        ret = lock_protocol::IOERR;
        break;
      case LOCKED:
        it->second.state = FREE;
        it->second.owner = "";
        break;
      case LOCKED_AND_WAIT:
        it->second.state = RETRYING;
        it->second.owner = "";
        retry = true;
        client_need_retry = *it->second.waitSet.begin();
        break;
      case RETRYING:
        ret = lock_protocol::IOERR;
        break;

      default:
        break;
    }
  }
  lck.unlock();
  if (retry) {
    int r;
    handle(client_need_retry).safebind()->call(rlock_protocol::retry, lid, r);
  }
  return ret;
}
