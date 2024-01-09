// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"

#include <stdio.h>

#include <iostream>
#include <sstream>

#include "rpc.h"
#include "tprintf.h"

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu) {
  srand(time(NULL) ^ last_port);
  rlock_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
  pthread_mutex_init(&m_mutex, NULL);
}

lock_protocol::status lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  int ret = lock_protocol::OK, r;

  pthread_mutex_lock(&m_mutex);

  auto it = m_lockMap.find(lid);

  if (it == m_lockMap.end()) {
    it = m_lockMap.insert(std::make_pair(lid, lock_entry())).first;
  }

  switch (it->second.state) {
    case NONE:
      it->second.state = ACQUIRING;
      it->second.retry = false;
      pthread_mutex_unlock(&m_mutex);
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      pthread_mutex_lock(&m_mutex);
      if (ret == lock_protocol::OK) {
        it->second.state = LOCKED;
        pthread_mutex_unlock(&m_mutex);
        return ret;
      } else if (ret == lock_protocol::RETRY) {
        if (!it->second.retry)
          pthread_cond_wait(&it->second.retryQueue, &m_mutex);
      }
      break;
    case FREE:
      it->second.state = LOCKED;
      pthread_mutex_unlock(&m_mutex);
      return lock_protocol::OK;
      break;
    case LOCKED:
      pthread_cond_wait(&it->second.waitQueue, &m_mutex);
      break;
    case ACQUIRING:
      if (!it->second.retry)
        pthread_cond_wait(&it->second.waitQueue, &m_mutex);
      else {
        pthread_mutex_unlock(&m_mutex);
        ret = cl->call(lock_protocol::acquire, lid, id, r);
        pthread_mutex_lock(&m_mutex);
        if (ret == lock_protocol::OK) {
          it->second.state = LOCKED;
          pthread_mutex_unlock(&m_mutex);
          return ret;
        } else if (ret == lock_protocol::RETRY) {
          if (!it->second.retry)
            pthread_cond_wait(&it->second.retryQueue, &m_mutex);
        }
      }
      break;
    case RELEASING:
      pthread_cond_wait(&it->second.releaseQueue, &m_mutex);
      break;

    default:
      break;
  }

  return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid) {
  int ret = lock_protocol::OK, r;

  pthread_mutex_lock(&m_mutex);

  auto it = m_lockMap.find(lid);

  if (it == m_lockMap.end()) {
    pthread_mutex_unlock(&m_mutex);
    return lock_protocol::NOENT;
  }

  if (it->second.revoke) {
    it->second.state = RELEASING;
    it->second.revoke = false;
    pthread_mutex_unlock(&m_mutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&m_mutex);
    it->second.state = NONE;
    pthread_cond_broadcast(&it->second.releaseQueue);
    pthread_mutex_unlock(&m_mutex);
    return ret;
  } else {
    it->second.state = FREE;
    pthread_cond_signal(&it->second.waitQueue);
    pthread_mutex_unlock(&m_mutex);
    return ret;
  }

  return ret;
}

rlock_protocol::status lock_client_cache::revoke_handler(
    lock_protocol::lockid_t lid, int &) {
  int ret = lock_protocol::OK, r;

  pthread_mutex_lock(&m_mutex);

  auto it = m_lockMap.find(lid);

  if (it == m_lockMap.end()) {
    pthread_mutex_unlock(&m_mutex);
    return lock_protocol::NOENT;
  }

  if (it->second.state == FREE) {
    it->second.state = RELEASING;
    pthread_mutex_unlock(&m_mutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&m_mutex);
    it->second.state = NONE;
    pthread_cond_broadcast(&it->second.releaseQueue);
    pthread_mutex_unlock(&m_mutex);
  } else {
    it->second.revoke = true;
    pthread_mutex_unlock(&m_mutex);
  }

  return ret;
}

rlock_protocol::status lock_client_cache::retry_handler(
    lock_protocol::lockid_t lid, int &) {
  int ret = lock_protocol::OK, r;

  pthread_mutex_lock(&m_mutex);

  auto it = m_lockMap.find(lid);

  if (it == m_lockMap.end()) {
    pthread_mutex_unlock(&m_mutex);
    return lock_protocol::NOENT;
  }

  it->second.retry = true;
  pthread_cond_signal(&it->second.retryQueue);
  pthread_mutex_unlock(&m_mutex);
  return ret;
}
