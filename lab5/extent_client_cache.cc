#include "extent_client_cache.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

// The calls assume that the caller holds a lock on the extent

extent_client_cache::extent_client_cache(std::string dst)
    : extent_client(dst){};

extent_protocol::status extent_client_cache::get(
    extent_protocol::extentid_t eid, std::string &buf) {
  std::lock_guard<std::mutex> lg(m_mutex);

  extent_protocol::status ret = extent_protocol::OK;
  auto it = m_dataMap.find(eid);
  if (it == m_dataMap.end()) {
    ret = cl->call(extent_protocol::get, eid, buf);
    if (ret == extent_protocol::OK) {
      m_dataMap[eid].data = buf;
      m_dataMap[eid].attr.size = buf.size();
      m_dataMap[eid].attr.atime = time(NULL);
      m_dataMap[eid].attr.ctime = m_dataMap[eid].attr.mtime = 0;
      m_dataMap[eid].status = UPDATE;
    }
  } else {
    switch (it->second.status) {
      case NONE:
        ret = cl->call(extent_protocol::get, eid, buf);
        if (ret == extent_protocol::OK) {
          m_dataMap[eid].data = buf;
          m_dataMap[eid].attr.size = buf.size();
          m_dataMap[eid].status = UPDATE;
          m_dataMap[eid].attr.atime = time(NULL);
        }
        break;
      case MODIFY:
      case UPDATE:
        buf = m_dataMap[eid].data;
        m_dataMap[eid].attr.atime = time(NULL);
        break;
      case REMOVE:
        ret = extent_protocol::NOENT;
        break;
    }
  }
  buf = m_dataMap[eid].data;
  return ret;
}

extent_protocol::status extent_client_cache::getattr(
    extent_protocol::extentid_t eid, extent_protocol::attr &attr) {
  std::lock_guard<std::mutex> lg(m_mutex);

  extent_protocol::status ret = extent_protocol::OK;
  auto it = m_dataMap.find(eid);
  if (it == m_dataMap.end()) {
    m_dataMap[eid].status = NONE;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    if (ret == extent_protocol::OK) {
      m_dataMap[eid].attr = attr;
    }
    return ret;
  } else {
    if (it->second.status == REMOVE) {
      ret = extent_protocol::NOENT;
    } else {
      attr = m_dataMap[eid].attr;
    }
    return ret;
  }
}

extent_protocol::status extent_client_cache::put(
    extent_protocol::extentid_t eid, std::string buf) {
  extent_protocol::status ret = extent_protocol::OK;
  int r;

  std::lock_guard<std::mutex> lg(m_mutex);
  auto it = m_dataMap.find(eid);
  if (it == m_dataMap.end()) {
    m_dataMap[eid].status = MODIFY;
    m_dataMap[eid].data = buf;
    m_dataMap[eid].attr.size = buf.size();
    m_dataMap[eid].attr.ctime = time(NULL);
    m_dataMap[eid].attr.mtime = time(NULL);
  } else {
    if (it->second.status == REMOVE) {
      return extent_protocol::NOENT;
    } else {
      m_dataMap[eid].status = MODIFY;
      m_dataMap[eid].data = buf;
      m_dataMap[eid].attr.size = buf.size();
      m_dataMap[eid].attr.ctime = time(NULL);
      m_dataMap[eid].attr.mtime = time(NULL);
    }
  }

  return ret;
}

extent_protocol::status extent_client_cache::remove(
    extent_protocol::extentid_t eid) {
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  std::lock_guard<std::mutex> lg(m_mutex);
  auto it = m_dataMap.find(eid);
  if (it == m_dataMap.end()) {
    //  m_dataMap[eid].status = REMOVE;
    ret = extent_protocol::NOENT;
  } else {
    if (m_dataMap[eid].status == REMOVE)
      ret = extent_protocol::NOENT;
    else
      m_dataMap[eid].status = REMOVE;
  }
  return ret;
}
extent_protocol::status extent_client_cache::flush(
    extent_protocol::extentid_t eid) {
  extent_protocol::status ret = extent_protocol::OK, r;

  std::lock_guard<std::mutex> lg(m_mutex);
  auto it = m_dataMap.find(eid);
  if (it == m_dataMap.end()) {
    ret = extent_protocol::NOENT;
  } else {
    if (m_dataMap[eid].status == REMOVE) {
      ret = cl->call(extent_protocol::remove, eid);
    } else if (m_dataMap[eid].status == MODIFY) {
      ret = cl->call(extent_protocol::put, eid, m_dataMap[eid].data, r);
    }
    m_dataMap.erase(eid);
  }

  return ret;
}
