// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server()
{
  int ret;
  put(1, "", ret);
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  m_mutex.lock();

  auto it = file_map.find(id);
  extent_protocol::attr attr;
  attr.atime = attr.ctime = attr.mtime = time(NULL);
  attr.size = buf.size();
  if (it != file_map.end())
    attr.atime = file_map[id].attr.atime;
  extent ex = {buf, attr};
  file_map[id] = ex;
  m_mutex.unlock();
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  m_mutex.lock();

  auto it = file_map.find(id);
  if (it != file_map.end())
  {
    file_map[id].attr.atime = time(NULL);
    buf = file_map[id].data;
    m_mutex.unlock();
    return extent_protocol::OK;
  }
  m_mutex.unlock();
  return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  m_mutex.lock();
  auto it = file_map.find(id);
  if (it != file_map.end())
  {
    a = file_map[id].attr;
    m_mutex.unlock();
    return extent_protocol::OK;
  }
  m_mutex.unlock();
  return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  m_mutex.lock();
  auto it = file_map.find(id);
  if (it != file_map.end())
  {
    file_map.erase(it);
    m_mutex.unlock();
    return extent_protocol::OK;
  }
  m_mutex.unlock();
  return extent_protocol::NOENT;
}
