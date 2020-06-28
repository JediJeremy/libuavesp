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
using UAVPortListener = std::function<void(SerialNodeID node_id, UAVInStream& in)>;
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
        uint16_t port_id;
        bool     is_input = false;
        bool     is_output = false;
        uint8_t  dtf_name_length;
        PGM_P    dtf_name;
        uint64_t dt_hash;
        UAVPortInfo(uint16_t port, PGM_P name);
};

// extra properties when being instanced in node
class UAVNodePortInfo : public UAVPortInfo {
    public: 
        uint64_t stats_emitted = 0;
        uint64_t stats_recieved = 0;
        uint64_t stats_errored = 0;
        // std::forward_list< std::function<void(UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply)> > on_request;
        std::forward_list<UAVPortFunction> on_request;
        UAVNodePortInfo(uint16_t port, PGM_P name) : UAVPortInfo{port,name} { }
};

class UAVPortList {
    private:
    public:
        std::map<uint16_t, UAVNodePortInfo*> list;
        UAVNodePortInfo* port_claim(uint16_t port_id, PGM_P dtf_name);
        // destructor
        ~UAVPortList();
        // port definition interface
        void define_subject(uint16_t subject_id, PGM_P dtf_name);
        void define_service(uint16_t service_id, PGM_P dtf_name, UAVPortFunction fn);
        // instance list on node
        void debug_ports();
};


class UAVNode {
    protected:
        // transport interfaces
        CanardInstance  _canard;
        std::vector<UAVSerialTransport *> _serial_transports;
        std::map<CanardPortID,SerialTransferID> _subject_tid;
        SerialTransferID next_subject_tid(CanardPortID port);
        std::map<std::tuple<CanardPortID,SerialNodeID>,SerialTransferID> _session_tid;
        SerialTransferID next_session_tid(CanardPortID port, SerialNodeID node_id);
        // task list
        int _task_timer = 0;
        std::vector<UAVTask *> _tasks;
        // service maps
        std::map< std::tuple<CanardPortID,SerialTransferID>, UAVPortRequest> _requests_inflight;
        std::multimap< uint32_t, std::tuple<CanardPortID,SerialTransferID>> _requests_timeout;
        std::map< std::tuple<SerialNodeID,CanardPortID>, UAVPortListener> _subscribe_nodeport;
        std::map< CanardPortID, UAVPortListener> _subscribe_port;
        std::map< std::tuple<CanardPortID,uint64_t>, UAVPortListener> _subscribe_portdata;
        // timeout management
        // std::multimap< uint32_t, void(*)()> _timer_events;
        void process_timeouts(uint32_t t1_ms, uint32_t t2_ms);
    public:
        UAVPortList ports;
        // public variables
        SerialNodeID serial_node_id = 0;
        int task_schedule = 10;
        std::function<uint64_t()> get_time_us;
        // con/destructor
        UAVNode();
        virtual ~UAVNode();
        // event loop
        void loop(const unsigned long t, const int dt);
        // transport management
        void serial_add(UAVSerialTransport *serial);
        void serial_remove(UAVSerialTransport *serial);
        void serial_receive(SerialTransfer *transfer);
        void debug_transfer(SerialTransfer *transfer);
        // task management
        void task_add(UAVTask *task);
        void task_remove(UAVTask *task);
        // publish and subscribe methods
        void publish(uint16_t subject_id, uint64_t datatype, CanardPriority priority, uint8_t * payload, int size, std::function<void()> callback);
        void publish(uint16_t subject_id, uint64_t datatype, CanardPriority priority, UAVOutStream& out, std::function<void()> callback);
        void request(uint16_t node_id, uint16_t service_id, uint64_t datatype, CanardPriority priority, uint8_t * payload, int size, UAVPortRequest callback);
        void request(uint16_t node_id, uint16_t service_id, uint64_t datatype, CanardPriority priority, UAVOutStream& out, UAVPortRequest callback);
        void respond(uint16_t node_id, uint16_t service_id, uint64_t transfer_id, uint64_t datatype, CanardPriority priority, uint8_t * payload, size_t size);
        void subscribe(uint16_t remote_node_id, uint16_t subject_id, UAVPortListener fn);
        void subscribe(uint16_t subject_id, UAVPortListener fn);
        void subscribe(uint16_t subject_id, uint64_t datatype, UAVPortListener fn);
        // datatype hash functions - used to turn arbitrary-length full datatype names into fixed-length integers with group-sortable semantics
        static uint64_t datatypehash(const char *name);
        static uint64_t datatypehash_P(PGM_P name, size_t size);
        static uint64_t datatypehash_P(PGM_P name);
        static uint64_t datatypehash(const char *root_ns, const char *subroot_ns, const char *dt_name, uint8_t version);
};


#endif
