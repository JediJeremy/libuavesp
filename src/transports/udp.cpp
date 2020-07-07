#include "udp.h"

// turn the UAVCAN port id into a UDP port number
uint16_t udp_port_number(UAVPortID port_id, UAVTransferKind kind) {
    uint16_t udp_port = 16384; // 1<<14
    switch(kind) {
        case UAVTransfer::KindMessage:  udp_port += (port_id & 0x7fff); break;
        case UAVTransfer::KindRequest:  udp_port -= (port_id & 0x0fff)*2 +2; break;
        case UAVTransfer::KindResponse: udp_port -= (port_id & 0x0fff)*2 +1; break;
    }
    return udp_port;
}

// turn a UDP port number back into a UAVCAN port id 
UAVPortID udp_port_id(uint16_t udp_port) {
    if(udp_port >= 16384) {
        return udp_port - 16384;
    }
    if(udp_port > 8192) {
        return 0x8000 | ((16384 - udp_port - 1) >> 1);
    }
    return 0;
}

// UDP transport
bool UDPTransport::start(UAVNode& node) {
    // create the common 'anonymous' port control for subject messages. link it to port 66.
    _pcb = udp_new();
    _pcb->local_port = 66;
    // calc the subnet and broadcast address. start from the local ip.
    local_ip = WiFi.localIP().v4();
    subnet_mask = WiFi.subnetMask().v4();
    subnet_ip = local_ip & subnet_mask;
    broadcast_ip = local_ip | ~subnet_mask;
    // 
    return true;
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
    ip_addr_t udp_addr = node_addr(transfer->remote_node_id);
    // turn the UAVCAN port id into a UDP port number
    uint16_t udp_port = udp_port_number(transfer->port_id, transfer->transfer_kind);
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
            t.transfer_kind = UAVTransfer::KindMessage;
            t.local_node_id = 0xffff; // it must have been broadcast
        } else if(udp_port > 8192) {
             t.port_id = ((16384 - udp_port - 1) >> 1) | 0x8000;
             t.transfer_kind = (udp_port&1)==1 ? UAVTransfer::KindResponse : UAVTransfer::KindRequest;
        }
        // give the decoded transfer frame to the node
        node.transfer_receive(&t);
    }
}


// UDP Transport using port listeners

bool PortUDPTransport::start(UAVNode& node) {
    // common udp startup
    if(UDPTransport::start(node)==false) return false;
    // setup any existing ports
    for(auto v : node.ports.list) {
        UAVNodePortInfo * info = v.second;
        port(node, info->port_id, info);
    }
    return true;
}

bool PortUDPTransport::stop(UAVNode& node) {
    // remove all active port liseners
    for(auto e : listeners) {
        auto cb = e.second;
        udp_remove(cb);
        delete cb;
    }
    return true;
}

udp_pcb* PortUDPTransport::port_bind(UAVNode& node, uint16_t udp_port, boolean bind) {
    if(udp_port==0) return nullptr;
    // check if we already had one
    udp_pcb* cb = listeners[udp_port];
    if(bind) {
        if(cb==nullptr) {
            // create one
            cb = udp_new();
            udp_recv(cb, &udp_recv_fn, (void *)&node);
            const ip_addr_t udp_addr = {
                .addr = local_ip
            };
            err_t err = udp_bind(cb, &udp_addr, udp_port);
            listeners[udp_port] = cb;
        }
        // we now definitely have one
        return cb;
    } else {
        if(cb!=nullptr) {
            // destroy it
            udp_remove(cb);
            delete cb;
            // listeners[udp_port] = nullptr;
            listeners.erase(udp_port);
        }
        // we now definitely don't have one
        return nullptr;
    }
}

// reconfigure port
void PortUDPTransport::port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) { 
    // what are the associated udp ports we need to maintain?
    uint16_t udp_in = 0;
    uint16_t udp_out = 0;
    boolean service = (port_id & 0x8000) != 0;
    if(service) {
        udp_in = udp_port_number(port_id, UAVTransfer::KindRequest);
        udp_out = udp_port_number(port_id, UAVTransfer::KindResponse);
    } else {
        udp_in = udp_port_number(port_id, UAVTransfer::KindMessage);
        udp_out = 0;
    }
    // debug
    if(info!=nullptr) {
        Serial.print(service ? "service #" : "subject #"); 
        Serial.print(info->port_id & 0x7FFF);
        Serial.print(" {");
        if(info->is_input)  { 
            Serial.print(" in:");  Serial.print(udp_in); 
        }
        if(info->is_output) {
            if(service) {
                Serial.print(" out:"); Serial.print(udp_out);
            } else {
                Serial.print(" out");
            }
        }
        Serial.print(" } ");
        Serial.print( FPSTR(info->dtf_name) );
        Serial.println();
    }
    // remove or add?
    if(info==nullptr) {
        // port was removed
        port_bind(node, udp_in, false);
        port_bind(node, udp_out, false);
    } else {
        // UAVPort may require udp port listeners...
        port_bind(node, udp_in, info->is_input);
        port_bind(node, udp_out, info->is_output);
    }
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
    // wrap the pbuf data in an input stream
    UAVInStream in( (uint8_t*)p->payload, p->len );
    // decode the frame stream to the node
    UDPTransport::decode_frame(*node, src_id, dst_id, pcb->local_port, in);
    // our responsibility to release the buffer
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
