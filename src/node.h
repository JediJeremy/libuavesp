#ifndef LIBUAVESP_NODE_H_INCLUDED
#define LIBUAVESP_NODE_H_INCLUDED

#include "common.h"
#include "transport.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include <forward_list>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#ifdef ESP_PLATFORM
  #include <WiFi.h>
#endif

class UAVTask {
    public:
        virtual void start(UAVNode& node);
        virtual void stop(UAVNode& node);
        virtual void loop(UAVNode& node, const unsigned long t, const int dt);
};

// 
using UAVPortListener = std::function<void(UAVNodeID node_id, UAVInStream& in)>;
using UAVPortRequest = std::function<void(UAVInStream& in)>;
using UAVPortReply = std::function<void(UAVOutStream& out)>;
//
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
        // instance list on node
        void debug_ports();
};


class UAVNode {
    protected:
        // transport interfaces
        std::vector<UAVTransport *> _transports;
        std::map< UAVPortID, UAVTransferID> _subject_tid;
        std::map< std::tuple<UAVPortID,UAVNodeID>, UAVTransferID> _session_tid;
        UAVTransferID next_subject_tid(UAVPortID port);
        UAVTransferID next_session_tid(UAVPortID port, UAVNodeID node_id);
        // task list
        int _task_timer = 0;
        std::vector<UAVTask *> _tasks;
        // service maps
        std::map< std::tuple<UAVPortID,UAVDatatypeHash>, UAVPortListener> _subscribe_portdata;
        std::map< std::tuple<UAVPortID,UAVTransferID>, UAVPortRequest> _requests_inflight;
        std::multimap< uint32_t, std::tuple<UAVPortID,UAVTransferID>> _requests_timeout;
        // timeout management
        void process_timeouts(uint32_t t1_ms, uint32_t t2_ms);
        void debug_transfer(UAVTransfer *transfer);
        // port management
        void port_update(UAVPortID port_id, UAVNodePortInfo* port_info);
    public:
        // public variables
        UAVNodeID local_node_id = 0;    // local node id
        UAVPortList ports;              // local node ports
        int task_schedule = 10;         // task schedule time
        std::function<uint64_t()> get_time_us; // microsecond time function
        // con/destructor
        UAVNode();
        virtual ~UAVNode();
        // setup IP address properties from a wifi connection
#ifdef ESP8266
        void set_id(ESP8266WiFiClass& wifi);
#endif
#ifdef ESP_PLATFORM
        void set_id(WiFiClass& wifi);
#endif
        // event loop
        void loop(const unsigned long t, const int dt);
        // transport management
        void add(UAVTransport *transport);
        void remove(UAVTransport *transport);
        void transfer_receive(UAVTransfer *transfer);
        // task management
        void add(UAVTask *task);
        void remove(UAVTask *task);
        // subject and service definition
        void define_subject(uint16_t subject_id, PGM_P dtf_name);
        void define_service(uint16_t service_id, PGM_P dtf_name, UAVPortFunction fn);
        // subject subscription
        void subscribe(UAVPortID subject_id, PGM_P dtf_name, UAVPortListener fn);
        // subject publishing
        void publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, std::function<void()> callback);
        void publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, std::function<void()> callback);
        // service request & response
        void request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, UAVPortRequest callback);
        void request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, UAVPortRequest callback);
        void respond(UAVNodeID node_id, UAVPortID service_id, UAVTransferID transfer_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, size_t size);
        // datatype hash functions - used to turn arbitrary-length full datatype names into fixed-length integers with group-sortable semantics
        static UAVDatatypeHash datatypehash_P(PGM_P name);
        static UAVDatatypeHash datatypehash_P(PGM_P name, size_t size);
        static UAVDatatypeHash datatypehash(const char *name);
        static UAVDatatypeHash datatypehash(const char *root_ns, const char *subroot_ns, const char *dt_name, uint8_t version);
};


#endif
