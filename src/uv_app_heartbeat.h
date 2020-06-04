#ifndef UV_APP_HEARTBEAT_H_INCLUDED
#define UV_APP_HEARTBEAT_H_INCLUDED

#include "common.h"
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
        uint8_t status[3]; // node status
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, HeartbeatMessage& v) { 
            s >> v.uptime; 
            s >> v.status[0] >> v.status[1] >> v.status[2];
            return s; 
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const HeartbeatMessage& v) { 
            s << v.uptime; 
            s << v.status[0] << v.status[1] << v.status[2];
            return s; 
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