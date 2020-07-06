#ifndef LIBUAVESP_APP_PORTINFO_H_INCLUDED
#define LIBUAVESP_APP_PORTINFO_H_INCLUDED

#include "common.h"
#include "node.h"
#include "primitive.h"
#include "nodeinfo.h"

static const     char dtname_uavcan_port_ID_1_0[] PROGMEM = "uavcan.port.ID.1.0";
static const uint64_t dthash_uavcan_port_ID_1_0 = UAVNode::datatypehash_P(dtname_uavcan_port_ID_1_0);
class PortID {
    public:
        // properties
        uint8_t type_tag;
        uint16_t port_id;
        bool is_subject() {
            return type_tag==0;
        }
        uint16_t as_subject() {
            return (type_tag==0) ? port_id >> 1 : 0;
        }
        void as_subject(uint16_t subject_id) {
            type_tag = 0;
            port_id = subject_id << 1;
        }
        bool is_service() {
            return type_tag==1;
        }
        uint16_t as_service() {
            return (type_tag==1) ? port_id >> 7 : 0;            
        }
        void as_service(uint16_t service_id) {
            type_tag = 1;
            port_id = service_id << 7;            
        }
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, PortID& v) {
            return s >> v.type_tag >> v.port_id;
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const PortID& v) {
            return s << v.type_tag << v.port_id;
        }
};



static const     char dtname_uavcan_port_GetInfo_1_0[] PROGMEM = "uavcan.port.GetInfo.1.0";
static const uint64_t dthash_uavcan_port_GetInfo_1_0 = UAVNode::datatypehash_P(dtname_uavcan_port_GetInfo_1_0);
static const uint16_t serviceid_uavcan_port_GetInfo_1_0 = 432;
class PortGetInfoRequest {
    public:
        PortID port_id;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, PortGetInfoRequest& v) { 
            return s >> v.port_id; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const PortGetInfoRequest& v) { 
            return s << v.port_id; 
        }
};
class PortGetInfoReply {
    public:
        bool is_input;
        bool is_output;
        std::string data_type_full_name;
        NodeVersion data_type_version;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, PortGetInfoReply& v) { 
            uint8_t is_flags;
            // decode the flags
            s >> is_flags; 
            v.is_input = (is_flags & 0x80)!=0;
            v.is_output = (is_flags & 0x40)!=0;
            // decode the rest
            return s >> v.data_type_full_name >> v.data_type_version; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const PortGetInfoReply& v) {
            // encode the flags into a byte 
            uint8_t is_flags = (v.is_input?0x80:0) | (v.is_output?0x40:0);
            // encode everything
            return s << is_flags << v.data_type_full_name << v.data_type_version; 
        }
};


static const     char dtname_uavcan_port_GetStatistics_1_0[] PROGMEM = "uavcan.port.GetStatistics.1.0";
static const uint64_t dthash_uavcan_port_GetStatistics_1_0 = UAVNode::datatypehash_P(dtname_uavcan_port_GetStatistics_1_0);
static const uint16_t serviceid_uavcan_port_GetStatistics_1_0 = 432;
class PortGetStatisticsRequest {
    public:
        PortID port_id;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, PortGetStatisticsRequest& v) { 
            return s >> v.port_id; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const PortGetStatisticsRequest& v) { 
            return s << v.port_id; 
        }
};
class PortGetStatisticsReply {
    public:
        NodeIOStatistics statistics;
        // stream parser & serializer
        friend UAVInStream& operator>>(UAVInStream& s, PortGetStatisticsReply& v) { 
            return s >> v.statistics; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const PortGetStatisticsReply& v) { 
            return s << v.statistics; 
        }
};


#endif