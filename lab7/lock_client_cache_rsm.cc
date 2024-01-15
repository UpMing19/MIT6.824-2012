// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

#include "rsm_client.h"

static void *releasethread(void *x) {
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *)x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst,
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
  rlsrpc->reg(rlock_protocol::revoke, this,
              &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this,
              &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC
  //   calls instead of the rpcc object of lock_client
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *)this);
  VERIFY(r == 0);
}

void lock_client_cache_rsm::releaser() {
  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.

  while (1) {
    release_entry e;
    releaseFifo.deq(&e);
    if (lu) lu->dorelease(e.lid);
    int r;
    cl->call(lock_protocol::release, e.lid, id, e.xid, r);
    std::unique_lock<std::mutex> lck(m_mutex);
    auto it = m_lockMap.find(e.lid);
    VERIFY(it != m_lockMap.end());
    it->second.status = NONE;
    releaseQueue.notify_all();
    waitQueue.notify_all();
    lck.unlock();
  }
}

lock_protocol::status lock_client_cache_rsm::acquire(
    lock_protocol::lockid_t lid) {
  int ret = lock_protocol::OK, r;

  std::unique_lock<std::mutex> lck(m_mutex);
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) {
    it = m_lockMap.insert(std::make_pair(lid, lock_entry())).first;
  }
  while (1) {
    switch (it->second.status) {
      case NONE:
        it->second.status = ACQUIRING;
        it->second.xid = xid;
        it->second.retry = false;
        xid++;
        lck.unlock();
        ret = cl->call(lock_protocol::acquire, lid, id, it->second.xid, r);
        lck.lock();
        if (ret == lock_protocol::OK) {
          it->second.status = LOCKED;
          return ret;
        } else if (ret == lock_protocol::RETRY) {
          if (it->second.retry == false) retryQueue.wait(lck);
        }

        break;

      case FREE:
        it->second.status = LOCKED;
        return ret;
        break;

      case LOCKED:
        waitQueue.wait(lck);
        break;

      case ACQUIRING:
        if (it->second.retry) {
          it->second.retry = false;
          it->second.xid = xid;
          xid++;
          lck.unlock();
          ret = cl->call(lock_protocol::acquire, lid, id, it->second.xid, r);
          lck.lock();
          if (ret == lock_protocol::OK) {
            it->second.status = LOCKED;
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            if (it->second.retry == false) retryQueue.wait(lck);
          }
        } else {
          waitQueue.wait(lck);
        }
        break;

      case RELEASING:
        releaseQueue.wait(lck);
        break;
    }
  }

  return ret;
}

lock_protocol::status lock_client_cache_rsm::release(
    lock_protocol::lockid_t lid) {
  int r;
  int ret = rlock_protocol::OK;
  std::unique_lock<std::mutex> lck(m_mutex);
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) return lock_protocol::NOENT;

  if (it->second.revoke) {
    it->second.status = RELEASING;
    it->second.revoke = false;
    lck.unlock();
    if (lu) lu->dorelease(lid);
    ret = cl->call(lock_protocol::release, lid, id, it->second.xid, r);
    lck.lock();
    it->second.status = NONE;
    releaseQueue.notify_all();
    waitQueue.notify_all();
  } else {
    it->second.status = FREE;
    waitQueue.notify_one();
  }

  return ret;
}

rlock_protocol::status lock_client_cache_rsm::revoke_handler(
    lock_protocol::lockid_t lid, lock_protocol::xid_t xid, int &) {
  int ret = rlock_protocol::OK;

  int r;
  std::unique_lock<std::mutex> lck(m_mutex);
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) return lock_protocol::NOENT;

  if (it->second.status == FREE) {
    it->second.status = RELEASING;
    releaseFifo.enq(release_entry(lid, xid));
  } else {
    it->second.revoke = true;
  }

  return ret;
}

rlock_protocol::status lock_client_cache_rsm::retry_handler(
    lock_protocol::lockid_t lid, lock_protocol::xid_t xid, int &) {
  int ret = rlock_protocol::OK;

  int r;
  std::unique_lock<std::mutex> lck(m_mutex);
  auto it = m_lockMap.find(lid);
  if (it == m_lockMap.end()) return lock_protocol::NOENT;
  VERIFY(it->second.xid == xid);
  it->second.retry = true;
  retryQueue.notify_one();
  return ret;
}
