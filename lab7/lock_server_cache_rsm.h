#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"

class lock_server_cache_rsm : public rsm_state_transfer
{

    enum lock_status
    {
        FREE,
        LOCKED,
        LOCKED_AND_WAIT,
        RETRYING
    };
    typedef std::map<std::string, int> client_reply_map;
    typedef std::map<std::string, lock_protocol::xid_t> client_xid_map;

    struct lock_entry
    {
        lock_status state = FREE;
        std::string owner;
        bool revoke = false;
        std::set<lock_protocol::lockid_t> waitset;
        client_xid_map highest_xid_from_client;
        client_reply_map highest_xid_acquire_reply;
        client_reply_map highest_xid_release_reply;
    };
    struct revoke_retry_entry
    {
        std::string id = "";
        lock_protocol::lockid_t lid = 0;
        lock_protocol::xid_t xid = 0;
    };
    std::map<lock_protocol::lockid_t, lock_entry> m_lockMap;
    std::mutex m_mutex;
    fifo<revoke_retry_entry> retryQueue;
    fifo<revoke_retry_entry> revokeQueue;

private:
    int nacquire;

    class rsm *rsm;

public:
    lock_server_cache_rsm(class rsm *rsm = 0);

    lock_protocol::status stat(lock_protocol::lockid_t, int &);

    void revoker();

    void retryer();

    std::string marshal_state();

    void unmarshal_state(std::string state);

    int acquire(lock_protocol::lockid_t, std::string id,
                lock_protocol::xid_t, int &);

    int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
                int &);
};

#endif
