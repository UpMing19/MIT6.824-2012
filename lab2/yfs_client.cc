// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool yfs_client::isfile(inum inum)
{
  if (inum & 0x80000000)
    return true;
  return false;
}

bool yfs_client::isdir(inum inum)
{
  return !isfile(inum);
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK)
  {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

  return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK)
  {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

release:
  return r;
}

yfs_client::inum
yfs_client::random_inum(bool isfile)
{
  inum ret = (unsigned long long)((rand() & 0x7fffffff) | (isfile << 31));
  ret = 0xffffffff & ret;
  return ret;
}
int yfs_client::create(inum parent, const char *name, inum &inum)
{
  int r = xxstatus::OK;
  std::string dir_data;
  std::string file_name;
  if (ec->get(parent, dir_data) != extent_protocol::OK)
  {
    r = IOERR;
    return r;
  }

  file_name = "/" + std::string(name) + "/";
  if (dir_data.find(file_name) != std::string::npos)
  {
    return EXIST;
  }
  inum = random_inum(true);
  if (ec->put(inum, std::string()) != extent_protocol::OK)
  {
    r = IOERR;
    return r;
  }
  dir_data.append(file_name + filename(inum) + "/");
  if (ec->put(parent, dir_data) != extent_protocol::OK)
  {
    r = IOERR;
    return r;
  }
  return r;
}

int yfs_client::lookup(inum parent, const char *name, inum &inum, bool *found)
{
  int r = xxstatus::OK;

  std::string dir_data;
  std::string file_name;
  std::string ino;
  if (ec->get(parent, dir_data) != extent_protocol::OK)
  {
    r = IOERR;
    return r;
  }
  file_name = "/" + std::string(name) + "/";
  if (dir_data.find(file_name) == std::string::npos)
  {
    r = IOERR;
    return r;
  }

  size_t pos, end;
  pos = dir_data.find(file_name);
  *found = true;
  pos += file_name.size();
  end = dir_data.find_first_of("/", pos);
  if (end != std::string::npos)
  {
    ino = dir_data.substr(pos, end - pos);
    inum = n2i(ino.c_str());
  }
  else
  {
    r = IOERR;
    return r;
  }
  return r;
}
int yfs_client::readdir(inum inum, std::list<dirent> &dirents)
{
  int r = xxstatus::OK;
  std::string dir_data;
  std::string inum_str;
  size_t pos, name_end, name_len, inum_end, inum_len;
  if (ec->get(inum, dir_data) != extent_protocol::OK)
  {
    r = IOERR;
    return r;
  }

  pos = 0;
  while (pos != dir_data.size())
  {
    dirent entry;
    pos = dir_data.find_first_of("/", pos);
    if (pos == dir_data.size())
      break;
    name_end = dir_data.find_first_of("/", pos + 1);
    entry.name = dir_data.substr(pos + 1, name_end - pos - 1);

    inum_end = dir_data.find_first_of("/", name_end + 1);
    inum_str = dir_data.substr(name_end + 1, inum_end - name_end - 1);
    entry.inum = n2i(inum_str.c_str());
    dirents.push_back(entry);
    pos = inum_end + 1;
  }
  return r;
}
