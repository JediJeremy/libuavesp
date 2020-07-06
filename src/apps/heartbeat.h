#ifndef LIBUAVESP_APP_HEARTBEAT_H_INCLUDED
#define LIBUAVESP_APP_HEARTBEAT_H_INCLUDED

#include "../common.h"
#include "../node.h"
#include "../primitive.h"

#define HEARTBEAT_PORT_ID           32085

#define HEARTBEAT_HEALTH_NOMINAL        0
#define HEARTBEAT_HEALTH_ADVISORY       1
#define HEARTBEAT_HEALTH_CAUTION        2
#define HEARTBEAT_HEALTH_WARNING        3

#define HEARTBEAT_MODE_OPERATIONAL      0
#define HEARTBEAT_MODE_INITIALIZATION   1
#define HEARTBEAT_MODE_MAINTENANCE      2
#define HEARTBEAT_MODE_SOFTWARE_UPDATE  3
#define HEARTBEAT_MODE_OFFLINE          7

#define HEARTBEAT_STATUS_NONE           0

#define HEARTBEAT_DELAY              1000


static const     char dtname_uavcan_node_Heartbeat_1_0[] PROGMEM = "uavcan.node.Heartbeat.1.0";
static const uint64_t dthash_uavcan_node_Heartbeat_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_Heartbeat_1_0);
static const uint16_t subjectid_uavcan_node_Heartbeat_1_0 = 32085;
class HeartbeatMessage {
    public:
        // properties
        uint32_t uptime;  // node uptime
        uint8_t health;
        uint8_t mode;
        uint32_t vendor;
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, HeartbeatMessage& v) { 
            uint8_t status[3];
            s >> v.uptime >> status[0] >> status[1] >> status[2];
            v.health = (status[0]>>6) & 0x03;
            v.mode = (status[0]>>3) & 0x07;
            v.vendor = ((uint32_t)status[0] & 0x07) << 16 | (uint32_t)status[1]<<8 | (uint32_t)status[2];
            return s; 
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const HeartbeatMessage& v) { 
            uint8_t status[3] = {
                ((v.health & 0x03)<<6) | ((v.mode & 0x07)<<3) | ((v.vendor & 0x070000)>>16),
                (v.vendor & 0x00ff00)>>8,
                (v.vendor & 0x0000ff)
            };
            return s << v.uptime << status[0] << status[1] << status[2]; 
        }
};

class HeartbeatApp : public UAVTask  {
    protected:
        HeartbeatMessage _message;
        unsigned long    _timer = 0;
    public:
        // application setup
        static void app_v1(UAVNode *node) {
            // we will be sending messages
            node->define_subject( subjectid_uavcan_node_Heartbeat_1_0, dtname_uavcan_node_Heartbeat_1_0 );
            // create a new app task
            node->add( new HeartbeatApp() );
        }
        // 
        void set_status(uint8_t health, uint8_t mode, uint32_t vendor);
        void send(UAVNode& node);
        // task interface
        void start(UAVNode& node) override;
        void loop(UAVNode& node, unsigned long t, int dt) override;
        void stop(UAVNode& node) override;
};

#endif