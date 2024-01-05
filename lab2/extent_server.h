// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include <mutex>

class extent_server
{

  struct extent
  {
    std::string data;
    extent_protocol::attr attr;
  };

public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

public:
  std::map<extent_protocol::extentid_t, extent> file_map;
  std::mutex m_mutex;
  
};

#endif
