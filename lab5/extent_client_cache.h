#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <mutex>
#include <string>

#include "extent_client.h"
#include "extent_protocol.h"
class extent_client_cache : public extent_client {
  enum file_status { NONE, UPDATE, MODIFY, REMOVE };
  struct extent {
    std::string data;
    extent_protocol::attr attr;
    file_status status = NONE;
  };

 public:
  extent_client_cache(std::string dst);
  extent_protocol::status get(extent_protocol::extentid_t eid,
                              std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status flush(extent_protocol::extentid_t eid);
  ~extent_client_cache();

 private:
  std::map<extent_protocol::extentid_t, extent> m_dataMap;
  std::mutex m_mutex;
};

#endif
