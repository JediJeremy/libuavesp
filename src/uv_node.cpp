#include "uv_node.h"
#include "uv_transport_serial.h"
#include "crc32c.h"

// memory allocator for canard
void* canard_allocate(CanardInstance* ins, size_t amount) { return malloc(amount); }
void canard_free(CanardInstance* ins, void* pointer) { free(pointer); }

// generic PortInfo object, used as common base class by lists
UAVPortInfo::UAVPortInfo(uint16_t port, PGM_P name) {
    port_id = port;
    // store the name
    dtf_name = name;
    // find length now, saves time later
    dtf_name_length = strlen_P(dtf_name);
    // compute the hash from the name
    dt_hash = UAVNode::datatypehash_P(dtf_name, dtf_name_length);
}

// portlists implement a concrete type of PortInfo
UAVPortList::~UAVPortList() {
    // delete the entries in our list
    for(auto pair : list) {
        delete pair.second;
    }
}

UAVNodePortInfo* UAVPortList::port_claim(uint16_t port_id, PGM_P dtf_name) {
    // is there an entry for this port already?
    UAVNodePortInfo* info = list[port_id];
    if(info==nullptr) {
        // no existing entry for this port, create it
        info = new UAVNodePortInfo(port_id,dtf_name);
        list[port_id] = info;
    }
    // return the info
    return info;
}

// set everything up in one go
void UAVPortList::define_publish(uint16_t port_id, PGM_P dtf_name) {
    UAVNodePortInfo* port_info = port_claim(port_id, dtf_name);
    if(port_info==nullptr) return;
    port_info->is_output = true;
}

// void UAVPortList::define_service(uint16_t port_id, PGM_P dtf_name, bool as_output, std::function<void(UAVNode * node, uint8_t* payload, int payload_size, UAVPortReply reply)> fn) {
void UAVPortList::define_service(uint16_t port_id, PGM_P dtf_name, bool as_output, UAVPortFunction fn) {
    UAVNodePortInfo* port_info = port_claim(port_id, dtf_name);
    if(port_info==nullptr) return;
    port_info->is_output |= as_output;
    if(fn!=nullptr) {
        port_info->is_input = true;
        port_info->on_request.push_front(fn);
    }
}

// show the ports that have been claimed
void UAVPortList::debug_ports() {
    for(auto e : list) {
        auto key = e.first;
        auto info = e.second;
        Serial.print(info->port_id); Serial.print(":");
        Serial.print(FPSTR(info->dtf_name)); Serial.println();
    }
}


//
UAVNode::UAVNode() {
    // simplest anonymous node
    serial_node_id = 0xFFFF;
}

UAVNode::~UAVNode() {
    // stop all transports
    for(SerialTransport * serial : _serial_transports) {
        serial->stop();
    }
}

/*
 * DataType Hash functions, aka Compact data type identifier
 * 
 * implemented according to https://forum.uavcan.org/t/alternative-transport-protocols/324
 * marked as pending review. may change in future.
 * 
 * Two functions provided, one that parses a full datatype name string into (root).(subroot).(tail).major.minor
 * the second takes 'pre-parsed' name fragments and avoids the parsing step.
 * 
 */
uint64_t UAVNode::datatypehash(const char *name) {
    // break up the full name into the root, subroot, and datatype name parts
    const char *part[4]; int size[4];
    int i=0; int len = strlen(name);
    int parts=0;
    int part_i=0; int part_c=0;
    while(i<len) {
        if(name[i]=='.') {
            if(parts==0) {
                // root part. easy.
                part[0]=name; size[0]=part_c;
            } else {
                if(parts==3) {
                    // keep the subroot part.
                    part[1] = part[2]; 
                    size[1] = size[2];
                }
                // shuffle the last part along
                part[2] = part[3]; 
                size[2] = size[3];
                // append the latest part
                part[3] = &name[part_i];
                size[3] = part_c;
            }
            parts++;
            part_i = i+1;
            part_c = 0;
        } else {
            part_c++;
        }
        i++;
    }
    // final part (minor version) is not included
    // if we have enough parts...
    if(parts>=3) {
        // if we don't have a subroot name
        if(parts==3) {
            // clear the subroot namespace part
            part[1] = NULL; 
            size[1] = 0;
        } else {
            // the remaining name part runs from the end of the subroot to the major version
            part[2] = part[1]+size[1]+1;
            size[2] = part[3]-part[2]-1;
        }
        int j;
        // hash the root namespace plus the special suffix
        char root_ns[size[0]+4];
        for(j=0; j<size[0]; j++) root_ns[j] = name[j];
        root_ns[j++] = 'c';
        root_ns[j++] = 'v';
        root_ns[j++] = 'o';
        root_ns[j++] = '0';
        uint32_t root_hash = crc32c((uint8_t *)root_ns, size[0]+4);
        // hash the subroot namespace, keep lower 12 bits
        uint32_t subroot_hash = crc32c((uint8_t *)part[1], size[1]) & 0xFFF;
        // hash the datatype name, keep lower 12 bits
        uint32_t dtname_hash = crc32c((uint8_t *)part[2], size[2]) & 0xFFF;
        // extract the major version
        char major[size[3]+1];
        for(j=0; j<size[0]; j++) major[j] = part[3][j];
        major[j++] = 0;
        uint8_t version = strtol(&major[0],NULL,10);
        // append them all together and return
        return ((uint64_t)root_hash<<32) | ((uint64_t)(subroot_hash<<20) | (dtname_hash<<8) | version);
    } else {
        // we cannot compute a data type hash
        return 0;
    }
}

uint64_t UAVNode::datatypehash_P(PGM_P name, size_t size) {
    // fill temporary RAM string from flash
    char dt_name[size+1];
    strncpy_P(dt_name, (PGM_P)name, size);
    dt_name[size] = 0;
    // compute the hash from the in-memory name
    return datatypehash(dt_name);
}

uint64_t UAVNode::datatypehash_P(PGM_P name) {
    return datatypehash_P(name, strlen_P((PGM_P)name));
}

uint64_t UAVNode::datatypehash(const char *root_ns, const char *subroot_ns, const char *dt_name, uint8_t version) {
    int j;
    // hash the root namespace plus the special suffix
    int root_len = strlen(root_ns);
    char s[root_len+4];
    for(j=0; j<root_len; j++) s[j] = root_ns[j];
    s[j++] = 'c';
    s[j++] = 'v';
    s[j++] = 'o';
    s[j++] = '0';
    uint32_t root_hash = crc32c((uint8_t *)s, root_len+4);
    // hash the subroot namespace, keep lower 12 bits
    uint32_t subroot_hash = subroot_ns==NULL ? 0 : crc32c((uint8_t *)subroot_ns, strlen(subroot_ns)) & 0xFFF;
    // hash the datatype name, keep lower 12 bits
    uint32_t dtname_hash = crc32c((uint8_t *)dt_name, strlen(dt_name)) & 0xFFF;
    // append them all together and return
    return ((uint64_t)root_hash<<32) | ((uint64_t)(subroot_hash<<20) | (dtname_hash<<8) | version);
}


void UAVNode::serial_add(SerialTransport *serial) {
    _serial_transports.push_back(serial);
    serial->start();
}

void UAVNode::serial_remove(SerialTransport *serial) {
    auto it = std::find(_serial_transports.begin(), _serial_transports.end(), serial);
    if(it==_serial_transports.end()) return;
    _serial_transports.erase(it);
    serial->stop();
}

void UAVNode::task_add(UAVTask *task) {
    _tasks.push_back(task);
    task->start(this);
}

void UAVNode::task_remove(UAVTask *task) {
    auto it = std::find(_tasks.begin(), _tasks.end(), task);
    if(it==_tasks.end()) return;
    _tasks.erase(it);
    task->stop(this);
}

SerialTransferID UAVNode::next_subject_tid(uint16_t port) {
    uint64_t counter;
    auto search = _subject_tid.find(port);
    if (search != _subject_tid.end()) {
        counter = search->second + 1;
        search->second = counter;
    } else {
        _subject_tid[port] = 0;
        counter = 0;
    }
    return counter;
}

SerialTransferID UAVNode::next_session_tid(CanardPortID port, SerialNodeID node_id) {
    uint64_t counter;
    auto key = std::make_tuple(port, node_id);
    auto search = _session_tid.find(key);
    if (search != _session_tid.end()) {
        counter = search->second + 1;
        search->second = counter;
    } else {
        _session_tid[key] = 0;
        counter = 0;
    }
    return counter;
}

void UAVNode::debug_transfer(SerialTransfer *transfer) {
    Serial.print("serial_receive{");
    Serial.print(" #"); if(transfer->remote_node_id==0xFFFF) { Serial.print("anon"); } else { Serial.print(transfer->remote_node_id); }
    Serial.print("->#"); if(transfer->local_node_id==0xFFFF) { Serial.print("anon"); } else { Serial.print(transfer->local_node_id); }
    Serial.print(":"); Serial.print(transfer->port_id);
    UAVPortInfo * port = ports.list[transfer->port_id];
    Serial.print(" "); Serial.print((uint32_t)(transfer->datatype>>32),16); Serial.print((uint32_t)(transfer->datatype),16);
    if(port!=nullptr) {
        Serial.print(":"); Serial.print(FPSTR(port->dtf_name));
    }
    // Serial.print(" "); Serial.print(transfer->transfer_kind);
    switch(transfer->transfer_kind) {
        case CanardTransferKindMessage: Serial.print(" message:"); break;
        case CanardTransferKindRequest: Serial.print(" request:"); break;
        case CanardTransferKindResponse: Serial.print(" response:"); break;
    }
    uint64_t tid = transfer->transfer_id;
    Serial.print((uint32_t)(tid>>32),16); Serial.print((uint32_t)(tid),16);
    Serial.print(" [");
    int n = min(8,(int)transfer->payload_size);
    uint8_t* data = (uint8_t*)(transfer->payload);
    for(int i=0; i<n; i++) {
        Serial.print(" ");
        Serial.print( data[i] ,16);
    }
    Serial.print((transfer->payload_size>n)? ".." : " ");
    Serial.print("]("); Serial.print(transfer->payload_size);
    Serial.println(")}");
}

void UAVNode::serial_receive(SerialTransfer *transfer) {
    debug_transfer(transfer);
    // was this message sent specifically to us?
    if(transfer->local_node_id==serial_node_id) {
        // do we have port functions waiting for this?
        UAVNodePortInfo * port = ports.list[transfer->port_id];
        if(port!=nullptr) {
            if(transfer->transfer_kind == CanardTransferKindRequest) {
                // create a reply handler for use by the functions.
                UAVPortReply reply = [transfer,this](uint8_t* payload, int payload_size)->void {
                    respond(transfer->remote_node_id, transfer->port_id, transfer->transfer_id, transfer->datatype, transfer->priority, payload, payload_size);
                };
                // give any port request handlers the chance to reply
                for(auto fn : port->on_request) {
                    fn(this, transfer->payload, transfer->payload_size, reply );
                }
            }
            if(transfer->transfer_kind == CanardTransferKindResponse) {
                // look up the request index
                auto it = _requests_inflight.find( std::make_tuple(transfer->port_id, transfer->transfer_id) );
                if(it==_requests_inflight.end()) {
                    // either the request was already fulfilled and this is a duplicate response (perhaps via redundant transports)
                    // or the request never existed, or we've rebooted in the time it took to return
                    /*
                    Serial.print("no inflight requests for ");
                    Serial.print(transfer->port_id);
                    Serial.print(" ");
                    Serial.print((uint32_t)transfer->transfer_id, 16);
                    Serial.println();
                    */
                } else {
                    // it was there, call it
                    auto callback = it->second;
                    callback(transfer->payload, transfer->payload_size);
                    // erase it from the index now it's complete
                    _requests_inflight.erase(it);
                } 
            }
        }
    } else if(transfer->local_node_id == 0xFFFF) {
        // subject broadcast. do we have any subscriptions for this?
        // ...
        
    } else {
        // this transfer wasn't a public broadcast or directly for us but we saw it anyway.
        // odds are we won't know what to do with it, except log it if we're in some kind
        // of promiscuous debugging mode
    }
    //
}

void UAVNode::process_timeouts(uint32_t t1_ms, uint32_t t2_ms) {
    // start an iterator on the requests
    auto it = _requests_timeout.lower_bound(t1_ms);
    auto it_end = _requests_timeout.end();
    while( it != it_end ) {
        if(it->first<=t2_ms) {
            // a request timed out. is it still inflight?
            auto key = it->second;
            auto e = _requests_inflight.find(key);
            if(e!=_requests_inflight.end()) {
                // it's still there. call back the request function FOR NOW with no data. this may split into an error path
                e->second(nullptr, 0);
                // we have dealt with the request
                _requests_inflight.erase(e);
            }
            // we have finished this, there may be more
            it = _requests_timeout.erase(it);
        } else {
            // too far future now. all done
            it = it_end;
        }
    }
}


void UAVNode::loop(const unsigned long t, const int dt) {
    // give time to each transport
    for(SerialTransport * serial : _serial_transports) {
        serial->loop(t,dt,this);
    }
    // if the millisecond timer has updated...
    if(dt>0) {
        // unfulfilled request timeouts. get the range to remove:
        uint32_t t1_ms = t - dt;
        uint32_t t2_ms = t;
        // if we cycled around
        if(t1_ms > t2_ms) {
            // then do it in two spans
            process_timeouts(t1_ms, 0xFFFF);
            process_timeouts(0, t2_ms);
        } else {
            // only takes one span
            process_timeouts(t1_ms, t2_ms);
        }
        // run tasks less often
        _task_timer += dt;
        if(_task_timer >= task_schedule) {
            for(UAVTask * task : _tasks) {
                task->loop(t,_task_timer,this);
            }
            _task_timer = 0;
        }
    }
}


void UAVNode::publish(uint16_t port, uint64_t datatype, CanardPriority priority, uint8_t* payload, size_t size, std::function<void()> callback) {
    // active CAN transport?
    /*
    if(_canard!=NULL) {
        // create a CAN transfer frame
        CanardTransfer *t = new CanardTransfer();
        t->timestamp_usec = 0;
        t->priority = priority;
        t->transfer_kind = CanardTransferKindMessage;
        t->port_id = port;
        t->remote_node_id = 0;
        t->payload_size = size;
        t->payload = payload;
        canardTxPush(&_canard, t);
    }
    */
    // active serial transports?
    if(_serial_transports.size()!=0) {
        // create a serial transfer header
        SerialTransfer* transfer = new SerialTransfer();
        // transfer header
        transfer->timestamp_usec = 0;
        transfer->priority = priority;
        transfer->transfer_kind = CanardTransferKindMessage;
        transfer->port_id = port;
        transfer->datatype = datatype;
        transfer->local_node_id = serial_node_id;
        transfer->remote_node_id = 0xFFFF; // anonymous id
        transfer->transfer_id = next_subject_tid(port);
        // payload
        transfer->payload_size = size;
        transfer->payload = payload;
        // frame properties
        transfer->frame_usage = 1;
        transfer->on_complete = callback;
        // encode the frame data
        SerialTransport::encode_frame(transfer,this);
        // enqueue it to each serial transport interface
        for(SerialTransport * serial : _serial_transports) {
            serial->send(transfer);
        }
        // release our usage, which might complete the transfer if it didn't actually make it into any queues
        transfer->frame_usage--;
        if(transfer->frame_usage == 0) SerialTransport::transfer_complete(transfer);
    }
}

void UAVNode::request(uint16_t node_id, uint16_t port, uint64_t datatype, CanardPriority priority, uint8_t* payload, size_t size, UAVPortReply callback) {
    // active CAN transport?
    /*
    if(_canard!=NULL) {
        // create a CAN transfer frame
        CanardTransfer *t = new CanardTransfer();
        t->timestamp_usec = 0;
        t->priority = priority;
        t->transfer_kind = CanardTransferKindRequest;
        t->port_id = port;
        t->remote_node_id = 0;
        t->payload_size = size;
        t->payload = payload;
        canardTxPush(&_canard, t);
    }
    */
    // active serial transports?
    if(_serial_transports.size()!=0) {
        // create a serial transfer
        SerialTransfer* transfer = new SerialTransfer();
        // transfer header
        transfer->timestamp_usec = 0;
        transfer->priority = priority;
        transfer->transfer_kind = CanardTransferKindRequest;
        transfer->port_id = port;
        transfer->datatype = datatype;
        transfer->local_node_id = serial_node_id;
        transfer->remote_node_id = node_id;
        transfer->transfer_id = next_session_tid(port, node_id);
        // payload
        transfer->payload_size = size;
        transfer->payload = payload;
        // frame properties
        transfer->frame_usage = 1;
        transfer->on_complete = nullptr;
        // encode the frame data
        SerialTransport::encode_frame(transfer,this);
        // enqueue it to each serial transport interface
        for(SerialTransport * serial : _serial_transports) {
            serial->send(transfer);
        }
        // release our usage, which might complete the transfer if it didn't actually make it into any queues
        transfer->frame_usage--;
        if(transfer->frame_usage == 0) {
            // none of the transports were able to accept it right now, treat it as complete
            SerialTransport::transfer_complete(transfer);
            // callback with empty reply (FOR NOW... recommend splitting this into an eror path)
            if(callback) callback(nullptr,0);
        } else {
            // put the callback into the requests index
            auto key = std::make_tuple(transfer->port_id, transfer->transfer_id);
            _requests_inflight[key] = callback;
            // put an entry into the timeout list
            uint32_t timeout_delay = 2000;
            uint32_t timeout_ms = millis() + timeout_delay;
            // found an empty slot, store the timeout
            _requests_timeout.insert(std::make_pair(timeout_ms, key));
        }
    }
}


void UAVNode::respond(uint16_t node_id, uint16_t port, uint64_t transfer_id, uint64_t datatype, CanardPriority priority, uint8_t* payload, size_t size) {
    // active serial transports?
    if(_serial_transports.size()!=0) {
        // create a serial transfer
        SerialTransfer* transfer = new SerialTransfer();
        // transfer header
        transfer->timestamp_usec = 0;
        transfer->priority = priority;
        transfer->transfer_kind = CanardTransferKindResponse;
        transfer->port_id = port;
        transfer->datatype = datatype;
        transfer->local_node_id = serial_node_id;
        transfer->remote_node_id = node_id;
        transfer->transfer_id = transfer_id;
        // payload
        transfer->payload_size = size;
        transfer->payload = payload;
        // frame properties
        transfer->frame_usage = 1;
        transfer->on_complete = nullptr;
        // encode the frame data
        SerialTransport::encode_frame(transfer,this);
        // enqueue it to each serial transport interface
        for(SerialTransport * serial : _serial_transports) {
            serial->send(transfer);  
        }
        // release our usage, which might complete the transfer if it didn't actually make it into any queues
        transfer->frame_usage--;
        if(transfer->frame_usage == 0) SerialTransport::transfer_complete(transfer);
    }
}

void UAVNode::subscribe(uint16_t remote_node_id, uint16_t port, UAVPortReply fn) { 
    auto key = std::make_tuple(remote_node_id, port);
    _subscribe_nodeport[key] = fn;
}

void UAVNode::subscribe(uint16_t port, UAVPortReply fn) { 
    _subscribe_port[port] = fn;
}

void UAVNode::subscribe(uint16_t port, uint64_t datatype, UAVPortReply fn) { 
    auto key = std::make_tuple(port, datatype);
    _subscribe_portdata[key] = fn;
}

