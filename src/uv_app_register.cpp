#include "uv_app_register.h"


void RegisterApp::app_v1(UAVNode *node, RegisterList *registers) {
    // define the port functions for this app
    // node->ports.define_subject( serviceid_uavcan_register_Access_1_0, dtname_uavcan_register_Access_1_0 );

    // register.Access service call
    node->ports.define_service( 
        serviceid_uavcan_register_Access_1_0, 
        dtname_uavcan_register_Access_1_0, 
        [&registers](UAVNode& node, UAVInStream& in, UAVPortReply reply) {
        });
    // register.List service call
    node->ports.define_service(
        serviceid_uavcan_register_List_1_0, 
        dtname_uavcan_register_List_1_0, 
        [&registers](UAVNode& node, UAVInStream& in, UAVPortReply reply) {
            
        });
    
}

