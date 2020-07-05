#include "uv_transport_udp.h"


// serial transport methods
bool UDPTransport::start(UAVNode& node) {
    // create the nameless port control
    _pcb = udp_new();
    _pcb->local_port = 66;
    // calc the subnet and broadcast address. start from the local ip.
    local_ip = WiFi.localIP().v4();
    subnet_mask = WiFi.subnetMask().v4();
    subnet_ip = local_ip & subnet_mask;
    broadcast_ip = local_ip | ~subnet_mask;
    // fix to allow reception of broadcast packets - never tested
    // wifi_set_sleep_type(NONE_SLEEP_T); 
    return true;
}


// we need to turn the UAVCAN port id into a UDP port number
uint16_t udp_port_number(UAVPortID port_id, UAVTransferKind kind) {
    uint16_t udp_port = 16384; // 1<<14
    switch(kind) {
        case CanardTransferKindMessage:  udp_port += (port_id & 0x7fff); break;
        case CanardTransferKindRequest:  udp_port -= (port_id & 0x0fff)*2 +2; break;
        case CanardTransferKindResponse: udp_port -= (port_id & 0x0fff)*2 +1; break;
    }
    return udp_port;
}


// we need to turn a UDP port number back into the UAVCAN port id 
UAVPortID udp_port_id(uint16_t udp_port) {
    if(udp_port >= 16384) {
        return udp_port - 16384;
    }
    if(udp_port > 8192) {
        return 0x8000 | ((16384 - udp_port - 1) >> 1);
    }
    return 0;
}

bool UDPTransport::stop(UAVNode& node) {
    // deallocate the nameless port
    udp_remove(_pcb);
    _pcb = nullptr;
    return true;
}

ip_addr_t UDPTransport::node_addr(UAVNodeID node_id) {
    ip_addr_t  udp_addr;
    if(node_id==0xFFFF) {
        // use the broadcast address
        udp_addr.addr = broadcast_ip;
    } else {
        // start from the subnet address and mix in the node id.
        udp_addr.addr = subnet_ip | ( (node_id & 0xFF) << 24) | ( (node_id & 0xFF00) << 8);
    }
    return udp_addr;
}

void UDPTransport::send(UAVTransfer* transfer) {
    // turn the destination node id into a udp/ip address
    /* UAVNodeID node_id = transfer->remote_node_id;
    ip_addr_t  udp_addr;
    if(node_id==0xFFFF) {
        // use the broadcast address
        udp_addr.addr = broadcast_ip.v4();
    } else {
        // start from the subnet address and mix in the node id.
        udp_addr.addr = subnet_ip.v4() | ( (node_id & 0xFF) << 24) | ( (node_id & 0xFF00) << 8);
    } */
    ip_addr_t udp_addr = node_addr(transfer->remote_node_id);
    // turn the UAVCAN port id into a UDP port number
    uint16_t udp_port = udp_port_number(transfer->port_id, transfer->transfer_kind);
    /*
    Serial.print("[<<"); if(transfer->remote_node_id!=0xFFFF) Serial.print(transfer->remote_node_id);
    Serial.print(":"); Serial.print(udp_port);
    Serial.print(">>]");
    */
    // build a single-frame datagram from the payload
    int size = 24 + transfer->payload_size;
    // allocate a lwip buffer for the datagram
    pbuf* tx_dgram = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
    if(!tx_dgram){
        Serial.print("failed pbuf_alloc");
        return;
    }
    // wrap the lwip buffer in an output stream
    UAVOutStream s( reinterpret_cast<uint8_t*>(tx_dgram->payload), size);

    // build the datagram
    s << (uint8_t)0; // version
    s << (uint8_t)transfer->priority; // priority
    s << (uint16_t)0; // zero padding
    s << (uint32_t)0x8000; // frame_index_eot
    s << (uint64_t)transfer->transfer_id;
    s << (uint64_t)transfer->datatype; 
    s.output_memcpy(transfer->payload, transfer->payload_size);

    // send the complete udp datagram
    err_t err = udp_sendto(_pcb, tx_dgram, &udp_addr, udp_port);
    if (err != ERR_OK) {
        Serial.print("udp_sendto err="); Serial.println((int) err);
    }
    // we are done with the buffer
    pbuf_free(tx_dgram);
}

void UDPTransport::decode_frame(UAVNode& node, UAVNodeID src_node_id, UAVNodeID dst_node_id, uint16_t udp_port, UAVInStream& in) {
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
        UAVTransfer t;
        t.priority = priority;
        t.remote_node_id = src_node_id;
        t.local_node_id = dst_node_id;
        t.transfer_id = transfer_id;
        t.datatype = datatype;
        t.payload = &in.input_buffer[in.input_index];
        t.payload_size = in.input_size - in.input_index;
        // decode the udp port range back to port specifier
        if(udp_port >= 16384) {
            t.port_id = udp_port - 16384;
            t.transfer_kind = CanardTransferKindMessage;
            t.local_node_id = 0xffff; // it must have been broadcast
        } else if(udp_port > 8192) {
             t.port_id = ((16384 - udp_port - 1) >> 1) | 0x8000;
             t.transfer_kind = (udp_port&1)==1 ? CanardTransferKindResponse : CanardTransferKindRequest;
        }
        // give the decoded transfer frame to the node
        node.transfer_receive(&t);
    }
}


// UDP Transport using port listeners

bool PortUDPTransport::start(UAVNode& node) {
    // common udp startup
    if(UDPTransport::start(node)==false) return false;
    /*
    // add all the active port listeners
    for(auto v : node.ports.list) {
        UAVNodePortInfo * info = v.second;
        uint16_t udp_port;
        if((info->port_id & 0x8000)!=0) {
            Serial.print("Service "); Serial.print(info->port_id & 0x7FFF); 
            udp_port = udp_port_number(info->port_id & 0x7FFF, CanardTransferKindRequest);
        } else {
            Serial.print("Subject "); Serial.print(info->port_id); 
            udp_port = udp_port_number(info->port_id, CanardTransferKindMessage);
        }
        Serial.print(" { ");
        Serial.print(info->is_input ? "in " : "");
        Serial.print(info->is_output ? "out " : "");
        // Serial.print(PSTR((const char [])info->dtf_name));
        Serial.print(" udp_port:");
        Serial.print(udp_port);
        Serial.println(" }");
    }
    */
    return true;
}

void PortUDPTransport::port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) { 
    // what are the associated udp ports
    uint16_t udp_in = 0;
    uint16_t udp_out = 0;
    if((port_id & 0x8000)!=0) {
        udp_in = udp_port_number(port_id, CanardTransferKindRequest);
        udp_out = udp_port_number(port_id, CanardTransferKindResponse);
    } else {
        udp_in = udp_port_number(port_id, CanardTransferKindMessage);
        udp_out = 0;
    }
    // remove or add?
    if(info==nullptr) {
        // port was removed

    } else {
        // port is new to us
        if((info->port_id & 0x8000)!=0) {
            Serial.print("service #"); Serial.print(info->port_id & 0x7FFF);
        } else {
            Serial.print("subject #"); Serial.print(info->port_id);
        }
        Serial.print(" {");
        if(info->is_input)  { Serial.print(" in:");  Serial.print(udp_in);  }
        if(info->is_output) { Serial.print(" out:"); Serial.print(udp_out); }
        Serial.print(" }");
        Serial.println();
        // if the UAVPort requires a udp port listener...
        if(info->is_input) {
            // check if we already had one
            auto cb = listeners[udp_in];
            if(cb==nullptr) {
                // create one
                cb = udp_new();
                udp_recv(cb, &udp_recv_fn, (void *)&node);
                const ip_addr_t udp_addr = {
                    .addr = local_ip
                };
                err_t err = udp_bind(cb, &udp_addr, udp_in);
                listeners[udp_in] = cb;
            }
            // we now definitely have one
        }
        // if the UAVPort requires a udp port listener...
        if((info->is_output) && (udp_out!=0)) {
            // check if we already had one
            auto cb = listeners[udp_out];
            if(cb==nullptr) {
                // create one
                cb = udp_new();
                udp_recv(cb, &udp_recv_fn, (void *)&node);
                const ip_addr_t udp_addr = {
                    .addr = local_ip
                };
                err_t err = udp_bind(cb, &udp_addr, udp_out);
                listeners[udp_out] = cb;
            }
            // we now definitely have one
        }
    }
}

bool PortUDPTransport::stop(UAVNode& node) {
    // remove all active port liseners

    return true;
}


/** Function prototype for udp pcb receive callback functions
 * addr and port are in same byte order as in the pcb
 * The callback is responsible for freeing the pbuf
 * if it's not used any more.
 *
 * ATTENTION: Be aware that 'addr' might point into the pbuf 'p' so freeing this pbuf
 *            can make 'addr' invalid, too.
 *
 * @param arg user supplied argument (udp_pcb.recv_arg)
 * @param pcb the udp_pcb which received data
 * @param p the packet buffer that was received
 * @param addr the remote IP address from which the packet was received
 * @param port the remote port from which the packet was received
 */
void PortUDPTransport::udp_recv_fn(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    // use the arg as a UAVNode reference
    if(arg==nullptr) return;
    UAVNode* node = (UAVNode *)arg;
    // turn ip addresses into node ids
    uint32_t sub_mask = WiFi.subnetMask().v4();
    uint32_t src_ip = ~(sub_mask) & addr->addr;
    UAVNodeID src_id = (src_ip>>8)&0xff00 | (src_ip>>24)&0x00ff;
    uint32_t dst_ip = ~(sub_mask) & pcb->local_ip.addr;
    UAVNodeID dst_id = (dst_ip>>8)&0xff00 | (dst_ip>>24)&0x00ff;
    // debug for now
    /*
    Serial.print("udp_recv_fn {");
    //Serial.print(" addr:"); Serial.print(addr->addr,16);
    //Serial.print(" remote_ip:"); Serial.print(pcb->remote_ip.addr,16);
    //Serial.print(" local_ip:"); Serial.print(pcb->local_ip.addr,16);
    Serial.print(" udp_port:"); Serial.print(pcb->local_port);
    //Serial.print(" flags:"); Serial.print(pcb->flags,1);
    //Serial.print(" so_options:"); Serial.print(pcb->so_options,1);
    UAVPortID port_id = udp_port_id(pcb->local_port);
    // Serial.print(" port_id:"); Serial.print(port_id);
    if((port_id & 0x8000)==0) {
        Serial.print(" subject:"); Serial.print(port_id);
    } else {
        Serial.print(" service:"); Serial.print(port_id & 0x7fff);
    }
    Serial.print(" src:"); Serial.print(src_id);
    Serial.print(" dst:"); Serial.print(dst_id);
    Serial.print(" len:"); Serial.print(p->len);
    Serial.print(" }");
    Serial.println();
    */
    /*
    Serial.print("[>>"); if(src_id!=0xFFFF) Serial.print(src_id);
    Serial.print(":"); Serial.print(pcb->local_port);
    // Serial.print("."); Serial.print(port);
    Serial.print("<<]");
    */
    // wrap the pbuf data in an input stream
    UAVInStream in((uint8_t*)p->payload, p->len);
    // decode the frame stream, send to the node if valid
    UDPTransport::decode_frame(*node, src_id, dst_id, pcb->local_port, in);
    // our job to release the buffer
    pbuf_free(p);
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
