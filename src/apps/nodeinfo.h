#ifndef LIBUAVESP_APP_NODEINFO_H_INCLUDED
#define LIBUAVESP_APP_NODEINFO_H_INCLUDED

#include "../common.h"
#include "../node.h"
#include "../primitive.h"

static const     char dtname_uavcan_node_ID_1_0[] PROGMEM = "uavcan.node.ID.1.0";
static const uint64_t dthash_uavcan_node_ID_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_ID_1_0);
class NodeID {
    public:
        // properties
        uint16_t value;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeID& v) {
            return s >> v.value;
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeID& v) {
            return s << v.value;
        }
};

static const     char dtname_uavcan_node_IOStatistics_1_0[] PROGMEM = "uavcan.node.IOStatistics.0.1";
static const uint64_t dthash_uavcan_node_IOStatistics_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_IOStatistics_1_0);
class NodeIOStatistics {
    public:
        // properties
        uint64_t num_emitted;
        uint64_t num_received;
        uint64_t num_errored;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeIOStatistics& v) {
            // receive as uint40 - 5 byte integers. 
            // zero out old values then read into the low 5 bytes of each 8-byte uint64_t.
            v.num_emitted = 0;  s.input_memcpy(&v.num_emitted, 5);
            v.num_received = 0; s.input_memcpy(&v.num_received, 5);
            v.num_errored = 0;  s.input_memcpy(&v.num_errored, 5);
            return s;
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeIOStatistics& v) {
            // send as uint40 - 5 byte integers.
            s.output_memcpy(&v.num_emitted,5);
            s.output_memcpy(&v.num_received,5);
            s.output_memcpy(&v.num_errored,5);
            return s;
        }
};

static const     char dtname_uavcan_node_Version_1_0[] PROGMEM = "uavcan.node.Version.1.0";
static const uint64_t dthash_uavcan_node_Version_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_Version_1_0);
class NodeVersion {
    public:
        // properties
        uint8_t major;
        uint8_t minor;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeVersion& v) {
            return s >> v.major >> v.minor;
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeVersion& v) {
            return s << v.major << v.minor;
        }
};

static const     char dtname_uavcan_node_ExecuteCommand_1_0[] PROGMEM = "uavcan.node.ExecuteCommand.1.0";
static const uint64_t dthash_uavcan_node_ExecuteCommand_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_ExecuteCommand_1_0);
static const uint16_t serviceid_uavcan_node_ExecuteCommand_1_0 = 435;
class NodeExecuteCommandRequest {
    public:
        // properties
        uint16_t command;
        std::string parameter;
        // UAVPrimitiveString<uint8_t,112> parameter;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeExecuteCommandRequest& v) { 
            return s >> v.command >> v.parameter; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeExecuteCommandRequest& v) { 
            return s << v.command << v.parameter; 
        }
};

class NodeExecuteCommandReply {
    public:
        // properties
        uint8_t status;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeExecuteCommandReply& v) { 
            return s >> v.status; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeExecuteCommandReply& v) { 
            return s 
                << v.status
                << (uint8_t)0 // 6 bytes of void
                << (uint8_t)0
                << (uint8_t)0
                << (uint8_t)0
                << (uint8_t)0
                << (uint8_t)0;
        }
};

static const     char dtname_uavcan_node_GetInfo_1_0[] PROGMEM = "uavcan.node.GetInfo.1.0";
static const uint64_t dthash_uavcan_node_GetInfo_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_GetInfo_1_0);
static const uint16_t serviceid_uavcan_node_GetInfo_1_0 = 430;
class NodeGetInfoReply {
    public:
        NodeVersion protocol_version;
        NodeVersion hardware_version;
        NodeVersion software_version;
        uint64_t software_vcs_revision_id;
        uint8_t unique_id[16];
        // UAVPrimitiveString<uint8_t,50> name;
        std::string name;
        uint8_t software_image_crc_count;
        uint64_t software_image_crc;
        // UAVPrimitiveString<uint8_t,222>  certificate;
        std::string certificate;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, NodeGetInfoReply& v) { 
            s >> v.protocol_version; 
            s >> v.hardware_version; 
            s >> v.software_version; 
            s >> v.software_vcs_revision_id; 
            for(int i=0; i<16; i++) s >> v.unique_id[i]; 
            s >> v.name; 
            s >> v.software_image_crc_count;
            if(v.software_image_crc_count==1) {
                s >> v.software_image_crc;
            }
            s >> v.certificate; 
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeGetInfoReply& v) { 
            s << v.protocol_version; 
            s << v.hardware_version; 
            s << v.software_version; 
            s << v.software_vcs_revision_id; 
            for(int i=0; i<16; i++) s << v.unique_id[i]; 
            s << v.name; 
            s << v.software_image_crc_count; 
            if(v.software_image_crc_count==1) {
                s << v.software_image_crc;
            }
            s << v.certificate; 
            return s; 
        }
};

class NodeinfoApp {
    public:
        // define the port functions for this app
        static void service_GetInfo_v1(UAVNode& node, UAVInStream& in, UAVPortReply reply);
        static void service_ExecuteCommand_v1(UAVNode& node, UAVInStream& in, UAVPortReply reply);
        // start app on node
        static void app_v1(UAVNode *node);
        // request API
        static void GetInfo(UAVNode *node, uint32_t node_id, std::function<void(NodeGetInfoReply*)> fn);
        static void ExecuteCommand(UAVNode *node, uint32_t node_id, uint16_t command, char parameter[], std::function<void(NodeExecuteCommandReply*)> fn);
};


#endif