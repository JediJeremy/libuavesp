#ifndef UV_APP_HEARTBEAT_H_INCLUDED
#define UV_APP_HEARTBEAT_H_INCLUDED

#include "uv_common.h"
#include "uv_node.h"
#include "uv_primitive.h"

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

#define HEARTBEAT_DELAY             10000


static const     char dtname_uavcan_node_Heartbeat_1_0[] PROGMEM = "uavcan.node.Heartbeat.1.0";
static const uint64_t dthash_uavcan_node_Heartbeat_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_Heartbeat_1_0);
static const uint16_t portid_uavcan_node_Heartbeat_1_0 = 32085;
class HeartbeatMessage {
    public:
        // properties
        uint32_t uptime;  // node uptime
        // uint8_t status[3]; // node status
        uint8_t health;
        uint8_t mode;
        uint32_t vendor;
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, HeartbeatMessage& v) { 
            uint8_t  status_a;
            uint16_t status_b;
            s >> v.uptime >> status_a >> status_b;
            v.health = (status_a>>6) & 0x03;
            v.mode = (status_a>>3) & 0x07;
            v.health = ((uint32_t)status_a & 0x07) << 16 | (uint32_t)status_b;
            return s; 
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const HeartbeatMessage& v) { 
            uint8_t  status_a = ((v.health & 0x03)<<6) | ((v.mode & 0x07)<<3) | ((v.vendor & 0x070000)>>16);
            uint16_t status_b = v.vendor & 0x00ffff;
            return s << v.uptime << status_a << status_b; 
        }
};

class HeartbeatApp : public UAVTask  {
    protected:
        HeartbeatMessage _message;
        unsigned long    _timer = 0;
    public:
        // application setup
        static void app_v1(UAVNode *node) {
            // create a new app task context
            UAVTask * app = new HeartbeatApp();
            // define the port functions for this app
            node->ports.define_service( portid_uavcan_node_Heartbeat_1_0, dtname_uavcan_node_Heartbeat_1_0, true, nullptr );
            // the node will call the app tasks
            node->task_add(app);
        }
        // 
        void set_status(uint8_t health, uint8_t mode, uint32_t vendor);
        void send(UAVNode *node);
        // task interface
        void start(UAVNode *node) override;
        void stop(UAVNode *node) override;
        void loop(unsigned long t,int dt, UAVNode *node) override;
};

#endif