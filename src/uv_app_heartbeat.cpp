#include "uv_app_heartbeat.h"


void HeartbeatApp::set_status(uint8_t health, uint8_t mode, uint32_t vendor) {
    _message.health = health;
    _message.mode = mode;
    _message.vendor = vendor;
}

void HeartbeatApp::send(UAVNode *node) {
    // send the next heartbeat message
    _message.uptime = millis() / (unsigned long)1000;
    uint8_t payload[16];
    UAVOutStream stream(payload,16);
    stream << _message;
    node->publish(subjectid_uavcan_node_Heartbeat_1_0, dthash_uavcan_node_Heartbeat_1_0, CanardPriorityNominal, stream.output_buffer, stream.output_index, []() {
        // Serial.println("heartbeat sent!");
    });
}

void HeartbeatApp::start(UAVNode *node) {
    // send the initialization mode heartbeat
    set_status(HEARTBEAT_HEALTH_NOMINAL, HEARTBEAT_MODE_INITIALIZATION, HEARTBEAT_STATUS_NONE);
    send(node);
    // change to operational mode for next heartbeat
    set_status(HEARTBEAT_HEALTH_NOMINAL, HEARTBEAT_MODE_OPERATIONAL, 0x4241);
    _timer = HEARTBEAT_DELAY;
}

void HeartbeatApp::stop(UAVNode *node) {
    // send the offline mode heartbeat
    set_status(HEARTBEAT_HEALTH_NOMINAL, HEARTBEAT_MODE_OFFLINE, HEARTBEAT_STATUS_NONE);
    send(node);
}

void HeartbeatApp::loop(unsigned long t,int dt, UAVNode *node) {
    long delta = t - _timer;
    if(delta>0) {
        // send heartbeat
        send(node);
        // update timer to next second-aligned step
        unsigned long skip = delta / HEARTBEAT_DELAY;
        _timer += (skip+1)*HEARTBEAT_DELAY;
    }
}
