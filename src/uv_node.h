#ifndef UV_NODE_H_INCLUDED
#define UV_NODE_H_INCLUDED

#include "uv_common.h"
#include "uv_transport.h"
#include <canard.h>
#include <stdlib.h>
#include <vector>
#include <map>
// #include <list>
#include <forward_list>


class UAVTask {
    public:
        virtual void start(UAVNode * node);
        virtual void stop(UAVNode * node);
        virtual void loop(const unsigned long t, const int dt, UAVNode * node);
};

// if our portfunction needs to send a reply, that interface is provided through here
// using UAVPortReply = void (*) (uint8_t* payload, int payload_size);
// using UAVPortReply = std::function<void(uint8_t* payload, int payload_size)>;
using UAVPortListener = std::function<void(UAVNodeID node_id, UAVInStream& in)>;
using UAVPortRequest = std::function<void(UAVInStream& in)>;
using UAVPortReply = std::function<void(UAVOutStream& out)>;
// port function callback
// using UAVPortFunction = void (*) (UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply);
// using UAVPortFunction = std::function<void(UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply)>;
using UAVPortFunction = std::function<void(UAVNode& node, UAVInStream& in, UAVPortReply reply)>;

// generic properties for a port
class UAVPortInfo {
    public: 
        // common port properties
        UAVPortID       port_id;
        bool            is_input = false;
        bool            is_output = false;
        uint8_t         dtf_name_length;
        PGM_P           dtf_name;
        UAVDatatypeHash dt_hash;
        // constructor
        UAVPortInfo(UAVPortID port, PGM_P name);
};

// extra properties when being instanced in node
class UAVNodePortInfo : public UAVPortInfo {
    public: 
        uint64_t stats_emitted = 0;
        uint64_t stats_recieved = 0;
        uint64_t stats_errored = 0;
        // std::forward_list< std::function<void(UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply)> > on_request;
        std::forward_list<UAVPortFunction> on_request;
        UAVNodePortInfo(UAVPortID port, PGM_P name) : UAVPortInfo{port,name} { }
};

class UAVPortList {
    private:
    public:
        std::map<UAVPortID, UAVNodePortInfo*> list;
        UAVNodePortInfo* port_claim(UAVPortID port_id, PGM_P dtf_name);
        // destructor
        ~UAVPortList();
        // port definition interface
        // void define_subject(UAVPortID subject_id, PGM_P dtf_name);
        // void define_service(UAVPortID service_id, PGM_P dtf_name, UAVPortFunction fn);
        // instance list on node
        void debug_ports();
};


class UAVNode {
    protected:
        // transport interfaces
        CanardInstance  _canard;
        std::vector<UAVTransport *> _transports;
        std::map< UAVPortID, UAVTransferID> _subject_tid;
        std::map< std::tuple<UAVPortID,UAVNodeID>, UAVTransferID> _session_tid;
        UAVTransferID next_subject_tid(UAVPortID port);
        UAVTransferID next_session_tid(UAVPortID port, UAVNodeID node_id);
        // task list
        int _task_timer = 0;
        std::vector<UAVTask *> _tasks;
        // service maps
        std::map< std::tuple<UAVPortID,UAVTransferID>, UAVPortRequest> _requests_inflight;
        std::multimap< uint32_t, std::tuple<UAVPortID,UAVTransferID>> _requests_timeout;
        // std::map< std::tuple<UAVNodeID,UAVPortID>, UAVPortListener> _subscribe_nodeport;
        // std::map< UAVPortID, UAVPortListener> _subscribe_port;
        std::map< std::tuple<UAVPortID,UAVDatatypeHash>, UAVPortListener> _subscribe_portdata;
        // timeout management
        // std::multimap< uint32_t, void(*)()> _timer_events;
        void process_timeouts(uint32_t t1_ms, uint32_t t2_ms);
        void debug_transfer(UAVTransfer *transfer);
    public:
        // public variables
        UAVNodeID local_node_id = 0;    // local node id
        UAVPortList ports;              // local node ports
        int task_schedule = 10;         // task schedule time
        std::function<uint64_t()> get_time_us; // microsecond time function
        // con/destructor
        UAVNode();
        virtual ~UAVNode();
        // event loop
        void loop(const unsigned long t, const int dt);
        // transport management
        void transport_add(UAVTransport *transport);
        void transport_remove(UAVTransport *transport);
        void transfer_receive(UAVTransfer *transfer);
        // task management
        void task_add(UAVTask *task);
        void task_remove(UAVTask *task);
        // port management
        void port_update(UAVPortID port_id, UAVNodePortInfo* port_info);
        // subject and service definition
        void define_subject(uint16_t subject_id, PGM_P dtf_name);
        void define_service(uint16_t service_id, PGM_P dtf_name, UAVPortFunction fn);
        // publish and subscribe methods
        void publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, std::function<void()> callback);
        void publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, std::function<void()> callback);
        void request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, UAVPortRequest callback);
        void request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, UAVPortRequest callback);
        void respond(UAVNodeID node_id, UAVPortID service_id, UAVTransferID transfer_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, size_t size);
        // void subscribe(UAVNodeID remote_node_id, UAVPortID subject_id, UAVPortListener fn);
        // void subscribe(UAVPortID subject_id, UAVPortListener fn);
        // void subscribe(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPortListener fn);
        void subscribe(UAVPortID subject_id, PGM_P dtf_name, UAVPortListener fn);
        // datatype hash functions - used to turn arbitrary-length full datatype names into fixed-length integers with group-sortable semantics
        static UAVDatatypeHash datatypehash_P(PGM_P name);
        static UAVDatatypeHash datatypehash_P(PGM_P name, size_t size);
        static UAVDatatypeHash datatypehash(const char *name);
        static UAVDatatypeHash datatypehash(const char *root_ns, const char *subroot_ns, const char *dt_name, uint8_t version);
};


#endif
