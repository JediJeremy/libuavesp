#include "uv_app_nodeinfo.h"

void NodeinfoApp::service_GetInfo_v1(UAVNode& node, UAVInStream& in, UAVPortReply reply) {
    // prepare default reply
    NodeGetInfoReply r;
    r.protocol_version.major = 1;
    r.protocol_version.minor = 0;
    r.hardware_version.major = 82;
    r.hardware_version.minor = 12;
    r.software_version.major = 1;
    r.software_version.minor = 0;
    // use a temp stream to pack unique hardware id bytes
    UAVOutStream uid(r.unique_id,16);
    uid.P(PSTR("ESP-8266")) << ESP.getChipId() << ESP.getFlashChipId();
    // friendly name
    r.name.assign("ESP 8266");
    // fill the vcs revision id
    String md5 = ESP.getSketchMD5();
    uint64_t hash = 0;
    for(int i=0; i<16; i++) {
        char c = md5[i];
        hash = hash << 4;
        if((c>='0') & (c<='9')) hash |= (c-'0');
        if((c>='a') & (c<='f')) hash |= (c-'a'+10);
    }
    r.software_image_crc_count = 1;
    r.software_image_crc = hash;
    // stream the message into a buffer
    uint8_t buffer[256];
    UAVOutStream out(buffer,256);
    out << r;
    // send reply
    reply(out);
}

void NodeinfoApp::service_ExecuteCommand_v1(UAVNode& node, UAVInStream& in, UAVPortReply reply) {
    // prepare default reply
    NodeExecuteCommandReply r;
    r.status = 0; // success status. all the non-zero codes are errors
    // decode the request
    NodeExecuteCommandRequest request;
    in >> request;
    Serial.print("command["); Serial.print(request.command); Serial.print("] "); Serial.println(request.parameter.c_str());
    switch(request.command) {
        case 65535: // COMMAND_RESTART
        case 65534: // COMMAND_POWER_OFF
        case 65533: // COMMAND_BEGIN_SOFTWARE_UPDATE
        case 65532: // COMMAND_FACTORY_RESET
        case 65531: // COMMAND_EMERGENCY_STOP
        case 65530: // COMMAND_STORE_PERSISTENT_STATES
            r.status = 5; // STATUS_BAD_STATE
            break;
        default:
            r.status = 3; // STATUS_BAD_COMMAND
            break;
    }
    // send reply
    uint8_t buffer[32];
    UAVOutStream out(buffer,32);
    out << r;
    reply(out);
}

// define the port and functions for this app
void NodeinfoApp::app_v1(UAVNode *node) {
    // node.GetInfo service call
    node->define_service( 
        serviceid_uavcan_node_GetInfo_1_0,
        dtname_uavcan_node_GetInfo_1_0,
        service_GetInfo_v1
    );
    // node.ExecuteCommand service call
    node->define_service( 
        serviceid_uavcan_node_ExecuteCommand_1_0, 
        dtname_uavcan_node_ExecuteCommand_1_0, 
        service_ExecuteCommand_v1
    );
}


void NodeinfoApp::GetInfo(UAVNode* node, uint32_t node_id, std::function<void(NodeGetInfoReply*)> fn) {
    // send an empty request
    node->request(
        node_id, 
        serviceid_uavcan_node_GetInfo_1_0, 
        dthash_uavcan_node_GetInfo_1_0, 
        CanardPriorityNominal, 
        nullptr, 0, 
        [fn](UAVInStream& in) {
            if(fn==nullptr) return; // no function, no worries
            if(&in==nullptr) return fn(nullptr); // null data, likely from timeout
            // parse our reply object and then callback
            NodeGetInfoReply info;
            in >> info;
            fn(&info);
        }
    );
}

void NodeinfoApp::ExecuteCommand(UAVNode* node, uint32_t node_id, uint16_t command, char parameter[], std::function<void(NodeExecuteCommandReply*)> fn) {
    // create the request message
    NodeExecuteCommandRequest r;
    r.command = command;
    r.parameter.assign(parameter);
    // stream it to a payload output buffer
    uint8_t payload[256];
    UAVOutStream out(payload,256);
    out << r;
    // make the request
    node->request(
        node_id, 
        serviceid_uavcan_node_ExecuteCommand_1_0, 
        dthash_uavcan_node_ExecuteCommand_1_0, 
        CanardPriorityNominal, 
        out, 
        [fn](UAVInStream& in) {
            if(fn==nullptr) return; // no function, no worries
            if(&in==nullptr) return fn(nullptr); // null data, likely from timeout
            // parse our reply object and then callback
            NodeExecuteCommandReply reply;
            in >> reply;
            fn(&reply);
        }
    );
}
