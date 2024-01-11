// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include <map>

class extent_client {
 private:
  rpcc *cl;
  struct extent
  {
    std::string data;
    extent_protocol::attr attr;
  };
  
  std::map<extent_protocol::extentid_t,extent>m_dataMap;


 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
};

#endif 

