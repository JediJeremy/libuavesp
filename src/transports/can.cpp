#include "can.h"


// memory allocator for canard
void* canard_allocate(CanardInstance* ins, size_t amount) { return malloc(amount); }
void canard_free(CanardInstance* ins, void* pointer) { free(pointer); }

bool CanardTransport::start(UAVNode& node) {
    // common udp startup
    // if(UAVTransport::start(node)==false) return false;
    // setup any existing ports
    for(auto v : node.ports.list) {
        UAVNodePortInfo * info = v.second;
        port(node, info->port_id, info);
    }
    return true;
}

void CanardTransport::port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) { 
    // remove or add?
    if(info==nullptr) {
        // port was removed

    } else {
        // port is new to us
        
    }
}

bool CanardTransport::stop(UAVNode& node) {
    return true;
}


void CanardTransport::send(UAVTransfer* transfer) {
    // create a CAN transfer frame from the generic transfer
    CanardTransfer *ct = new CanardTransfer();
    ct->timestamp_usec = 0;
    ct->priority = (CanardPriority)transfer->priority;
    ct->transfer_kind = (CanardTransferKind)transfer->transfer_kind;
    ct->port_id = transfer->port_id;
    ct->transfer_id = transfer->transfer_id;
    ct->remote_node_id = transfer->remote_node_id;
    ct->payload_size = transfer->payload_size;
    ct->payload = transfer->payload;
    // push that to the canard stack
    canardTxPush(&_canard, ct);
}

