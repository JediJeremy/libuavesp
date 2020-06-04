#include "uv_app_nodeinfo.h"

void NodeinfoApp::GetInfo(UAVNode *node, uint32_t node_id, std::function<void(NodeGetInfoReply*)> fn) {
    // send the next heartbeat message
    node->request(node_id, portid_uavcan_node_GetInfo_1_0, dthash_uavcan_node_GetInfo_1_0, CanardPriorityNominal, nullptr, 0, [fn](uint8_t* payload, int payload_size) {
        if(payload_size) {
            UAVInStream input(payload, payload_size);
            NodeGetInfoReply info;
            input >> info;
            Serial.println("info {");
            Serial.print("     name: "); Serial.print(info.name.c_str()); Serial.println();
            Serial.print("      uid: ["); for(int i=0; i<16; i++) { Serial.print(info.unique_id[i],16); Serial.print(i==15?"]":" "); } Serial.println();
            Serial.print(" protocol: v"); Serial.print(info.protocol_version.major); Serial.print("."); Serial.print(info.protocol_version.minor); Serial.println();
            Serial.print(" hardware: v"); Serial.print(info.hardware_version.major); Serial.print("."); Serial.print(info.hardware_version.minor); Serial.println();
            Serial.print(" software: v"); Serial.print(info.software_version.major); Serial.print("."); Serial.print(info.software_version.minor); Serial.println();
            Serial.println("}");
            if(fn!=nullptr) fn(&info);
        } else {
            Serial.println("info {}");
            if(fn!=nullptr) fn(nullptr);
        }
    });
}


void NodeinfoApp::ExecuteCommand(UAVNode *node, uint32_t node_id, uint16_t command, char parameter[], std::function<void(NodeExecuteCommandReply*)> fn) {
    // send the next heartbeat message
    uint8_t payload[256];
    UAVOutStream stream(payload,256);
    NodeExecuteCommandRequest req;
    req.command = command;
    req.parameter.assign(parameter);
    stream << req;
    node->request(node_id, portid_uavcan_node_ExecuteCommand_1_0, dthash_uavcan_node_ExecuteCommand_1_0, CanardPriorityNominal, stream.output_buffer, stream.output_index, [](uint8_t* payload, int payload_size) {
        Serial.print("order confirmed. ");
        Serial.print(payload_size);
        Serial.println();
    });
}

// define the port and functions for this app
void NodeinfoApp::app_v1(UAVNode *node) {
    // node.GetInfo service call
    node->ports.define_service( 
        portid_uavcan_node_GetInfo_1_0,
        dtname_uavcan_node_GetInfo_1_0,
        true,
        [](UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply) {
            // Serial.println("GetInfo request recieved!");
            // prepare default reply
            NodeGetInfoReply res;
            res.protocol_version.major = 1;
            res.protocol_version.minor = 0;
            res.hardware_version.major = 82;
            res.hardware_version.minor = 12;
            res.software_version.major = 1;
            res.software_version.minor = 0;
            
            // use a temp stream to pack unique hardware id bytes
            UAVOutStream unique_id(res.unique_id,16);
            unique_id.P(PSTR("ESP-8266"));
            unique_id << ESP.getChipId();
            unique_id << ESP.getFlashChipId();
            // friendly name
            res.name.assign("ESP 8266");
            // fill the vcs revision id
            String md5 = ESP.getSketchMD5();
            uint64_t hash = 0;
            for(int i=0; i<16; i++) {
                char c = md5[i];
                hash = hash << 4;
                if((c>='0') & (c<='9')) hash |= (c-'0');
                if((c>='a') & (c<='f')) hash |= (c-'a'+10);
            }
            res.software_image_crc_count = 1;
            res.software_image_crc = hash;
            // stream the message into a buffer
            uint8_t buffer[256];
            UAVOutStream stream(buffer,256);
            stream << res;
            // send reply
            reply(buffer,stream.output_index);
        });
    // node.ExecuteCommand service call
    node->ports.define_service( 
        portid_uavcan_node_ExecuteCommand_1_0, 
        dtname_uavcan_node_ExecuteCommand_1_0, 
        true,
        [](UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply) {
            // Serial.println("ExecuteCommand request recieved!");
            // prepare default reply
            NodeExecuteCommandReply res;
            res.status = 0; // success status. all the non-zero codes are errors
            // decode the request
            NodeExecuteCommandRequest req;
            UAVInStream input(payload, payload_size);
            input >> req;
            Serial.print("command["); Serial.print(req.command); Serial.print("] "); Serial.println(req.parameter.c_str());
            switch(req.command) {
                case 65535: // COMMAND_RESTART
                case 65534: // COMMAND_POWER_OFF
                case 65533: // COMMAND_BEGIN_SOFTWARE_UPDATE
                case 65532: // COMMAND_FACTORY_RESET
                case 65531: // COMMAND_EMERGENCY_STOP
                case 65530: // COMMAND_STORE_PERSISTENT_STATES
                    res.status = 5; // STATUS_BAD_STATE
                    break;
                default:
                    res.status = 3; // STATUS_BAD_COMMAND
                    break;
            }
            // send reply
            uint8_t buffer[32];
            UAVOutStream writer(buffer,32);
            writer << res;
            reply(buffer,writer.output_index);
        });
}

