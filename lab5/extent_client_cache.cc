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
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::get, eid, buf);
  return ret;
}

extent_protocol::status extent_client_cache::getattr(
    extent_protocol::extentid_t eid, extent_protocol::attr &attr) {
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status extent_client_cache::put(
    extent_protocol::extentid_t eid, std::string buf) {
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}

extent_protocol::status extent_client_cache::remove(
    extent_protocol::extentid_t eid) {
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);
  return ret;
}
extent_protocol::status extent_client_cache::flush(
    extent_protocol::extentid_t eid) {
  extent_protocol::status ret = extent_protocol::OK;

  return ret;
}
