#include "uv_transport_udp.h"


// serial transport methods
bool UDPTransport::start(UAVNode& node) {
    // set the broadcast address. start from the local ip.
    broadcast_ip = WiFi.localIP();
    subnet_ip = WiFi.localIP();
    // mask _in_ the subnet mask's zeros
    IPAddress mask = WiFi.subnetMask();
    for(int i=0; i<4; i++) broadcast_ip[i] |= ~mask[i];
    for(int i=0; i<4; i++) subnet_ip[i] &= mask[i];
    //
    // fix to allow reception of broadcast packets
    // wifi_set_sleep_type(NONE_SLEEP_T); 
    return true;
}

uint16_t udp_port_number(uint16_t port_id, CanardTransferKind kind) {
        // we need to turn the UAVCAN port id into a UDP port number
    uint16_t udp_port = 16384; // 1<<14
    switch(kind) {
        case CanardTransferKindMessage:  udp_port += port_id; break;
        case CanardTransferKindRequest:  udp_port -= port_id *2+2; break;
        case CanardTransferKindResponse: udp_port -= port_id *2+1; break;
    }
    return udp_port;
}

bool UDPTransport::stop(UAVNode& node) {
    return true;
}

void UDPTransport::send(SerialTransfer* transfer) {
    SerialNodeID node_id = transfer->remote_node_id;
    IPAddress node_ip;
    if(node_id=0xFFFF) {
        // use the broadcast address
        node_ip = broadcast_ip;
    } else {
        // start from the subnet address and add in the node id.
        node_ip = subnet_ip;
        node_ip[2] |= (node_id>>8) & 0xff;
        node_ip[3] |= node_id & 0xff;
    }
    // turn the UAVCAN port id into a UDP port number
    uint16_t udp_port = udp_port_number(transfer->port_id, transfer->transfer_kind);
    // build a full datagram
    int size = 24 + transfer->payload_size;
    uint8_t datagram[size];
    UAVOutStream s(datagram, size);
    s << (uint8_t)0; // version
    s << (uint8_t)transfer->priority; // priority
    s << (uint16_t)0; // zero padding
    s << (uint32_t)0x8000; // frame_index_eot
    s << (uint64_t)transfer->transfer_id;
    s << (uint64_t)transfer->datatype; 
    s.output_memcpy(transfer->payload, transfer->payload_size);
    // define a UDP connection state
    esp_udp cn_udp;
    cn_udp.remote_port = udp_port;
    for(int i=0; i<4; i++) cn_udp.remote_ip[i] = node_ip[i];
    espconn cn;
    cn.type = ESPCONN_UDP;
    cn.state = ESPCONN_NONE;
    cn.proto.udp = &cn_udp;
    // send the datagran
    
    espconn_create(&cn);
    // espconn_sendto(&cn, datagram, size);
    espconn_delete(&cn);
    
    /*
    udp.beginPacket(node_ip, udp_port);
    udp.write(packet_header, 24);
    udp.write(transfer->payload, transfer->payload_size);
    udp.endPacket();
    */
}

void UDPTransport::decode_frame(UAVNode& node, IPAddress node_ip, uint16_t udp_port, UAVInStream& in) {
    uint8_t version;
    uint8_t priority;
    uint16_t void1;
    uint32_t frame_index_eot;
    uint64_t transfer_id;
    uint64_t datatype; 
    in >> version;
    if(version==0) {
        in >> priority >> void1 >> frame_index_eot >> transfer_id >> datatype;
        if(frame_index_eot!=0x8000) return; // cannot currently handle multi-frame transfers
        // create a transfer object
        SerialTransfer t;
        t.priority = (CanardPriority)priority;
        t.transfer_id = transfer_id;
        t.datatype = datatype;
        t.payload = &in.input_buffer[in.input_index];
        t.payload_size = in.input_size - in.input_index;
        // decode the udp port range back to port specifier
        int pv = (int)udp_port - (1<<14);
        if(pv>=0) {
            t.port_id = pv;
            t.transfer_kind = CanardTransferKindMessage;
        } else {
            t.port_id = (-pv)>>1 + 1;
            t.transfer_kind = (pv&1)==1 ? CanardTransferKindResponse : CanardTransferKindRequest;
        }
        // give the decoded transfer frame to the node
        node.serial_receive(&t);
    }
}


// UDP Transport using port listeners

bool PortUDPTransport::start(UAVNode& node) {
    // common udp startup
    if(UDPTransport::start(node)==false) return false;
    // add all the active port listeners
    for(auto v : node.ports.list) {
        UAVPortInfo * info = v.second;
        Serial.print("Port "); Serial.print(info->port_id); Serial.print(" { ");
        Serial.print(info->is_input ? "in " : "");
        Serial.print(info->is_output ? "out " : "");
        // Serial.print(PSTR((const char [])info->dtf_name));
        Serial.println(" }");
        /*
        if(info->is_output) {
            auto udp_port = udp_port_number(info->port_id, CanardTransferKindMessage);
        }
        if(info->is_input) {
            auto udp_port = udp_port_number(info->port_id, CanardTransferKindRequest);
        }
        */
    }
    return true;
}

bool PortUDPTransport::stop(UAVNode& node) {
    // remove all active port liseners

    return true;
}


// UDP Transport using promiscuous-mode packet reciever

bool PromiscousUDPTransport::start(UAVNode& node) {
    // common udp startup
    if(UDPTransport::start(node)==false) return false;
    // enable promiscuous mode

    return true;
}

bool PromiscousUDPTransport::stop(UAVNode& node) {
    // stop the listeners

    return true;
}
