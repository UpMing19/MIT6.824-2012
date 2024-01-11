// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;

  auto it = m_dataMap.find(eid);
  if(it==m_dataMap.end()){
      ret = cl->call(extent_protocol::get, eid, buf);
      m_dataMap[eid].attr.atime = time(NULL);
      m_dataMap[eid].data = buf;
      return ret;
  }
  else{
      buf = it->second.data;
      return ret;
  }
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  auto it = m_dataMap.find(eid);
  if(it==m_dataMap.end()){
      ret = extent_protocol::NOENT;
  }
  else{
      attr = it->second.attr;
      return ret;
  }

  ret = cl->call(extent_protocol::getattr, eid, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  extent_protocol::attr attr;
  attr.atime = attr.mtime = attr.ctime = time(NULL);
  m_dataMap[eid].data = buf;
  m_dataMap[eid].attr = attr;
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  m_dataMap.clear();
  return ret;
}


