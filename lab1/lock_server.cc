// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock::lock(lock_protocol::lockid_t lid, lock_status lockStatus) {
    m_lockid = lid;
    m_lockStatus = lockStatus;
}

lock_server::lock_server() {}

lock_server::~lock_server() {
//    for (auto it = m_lockMap.begin(); it != m_lockMap.end(); it++)
//        delete it->second;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
    lock_protocol::status ret = lock_protocol::OK;
    printf("stat request from clt %d\n", clt);
    r = nacquire;
    return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &) {

    lock_protocol::status ret = lock_protocol::OK;
    std::unique_lock <std::mutex> uniqueLock(m_mutex);

    while (1) {
        auto it = m_lockMap.find(lid);
        if (it == m_lockMap.end()) {
            lock *l = new lock(lid, lock::lock_status::LOCKED);
            m_lockMap[lid] = l;
            break;
        } else {
            if (it->second->m_lockStatus != lock::FREE) {
                it->second->m_cv.wait(uniqueLock);
            } else {
                it->second->m_lockStatus = lock::LOCKED;
                break;
            }
        }
    }
    uniqueLock.unlock();
    return ret;

}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &) {
    lock_protocol::status ret = lock_protocol::xxstatus::OK;
    auto it = m_lockMap.find(lid);
    std::unique_lock <std::mutex> uniqueLock(m_mutex);
    if (it == m_lockMap.end()) {
        return lock_protocol::NOENT;
    } else {
        //   uniqueLock.lock();
        it->second->m_lockStatus = lock::FREE;
        it->second->m_cv.notify_all();
        //  m_lockMap.erase(it);
        uniqueLock.unlock();
        return ret;
    }
}
