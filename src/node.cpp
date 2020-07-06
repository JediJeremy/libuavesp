#include "node.h"
#include "crc32c.h"

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
    local_node_id = 0xFFFF;
}

UAVNode::~UAVNode() {
    // stop all remaining transports
    for(auto t : _transports) t->stop(*this);
}

// declare that we will be listening for subjects of this datatype
void UAVNode::subscribe(UAVPortID subject_id, PGM_P dtf_name, UAVPortListener fn) { 
    // claim port info
    auto info = ports.port_claim( subject_id, dtf_name );
    if(info==nullptr) return;
    if(info->is_input == false) {
        // set as input type
        info->is_input = true;
        // notify transports of port creation
        for(auto t : _transports) t->port(*this, subject_id, info);
    }
    // add the function to the listeners list
    if(fn!=nullptr) {
        auto key = std::make_tuple(subject_id, info->dt_hash);
        _subscribe_portdata[key] = fn;
    }
}

// declare that we will be emitting subjects of this datatype
void UAVNode::define_subject(uint16_t subject_id, PGM_P dtf_name) {
    UAVNodePortInfo* port_info = ports.port_claim(subject_id, dtf_name);
    if(port_info==nullptr) return;
    if(!port_info->is_output) {
        // modify into an output
        port_info->is_output = true;
        // let everyone know
        for(auto t : _transports) t->port(*this, subject_id, port_info);
    }
}

// declare that we will be providing a service of this datatype
void UAVNode::define_service(uint16_t service_id, PGM_P dtf_name, UAVPortFunction fn) {
    UAVPortID port_id = service_id | 0x8000;
    UAVNodePortInfo* port_info = ports.port_claim(port_id, dtf_name);
    if(port_info==nullptr) return;
    if(fn!=nullptr) {
        if(!port_info->is_input) {
            // modify into a service
            port_info->is_input = true;
            port_info->is_output = true;
            // let everyone know
            for(auto t : _transports) t->port(*this, port_id, port_info);
        }
        port_info->on_request.push_front(fn);
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
UAVDatatypeHash UAVNode::datatypehash(const char *name) {
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

UAVDatatypeHash UAVNode::datatypehash_P(PGM_P name, size_t size) {
    // fill temporary RAM string from flash
    char dt_name[size+1];
    strncpy_P(dt_name, (PGM_P)name, size);
    dt_name[size] = 0;
    // compute the hash from the in-memory name
    return datatypehash(dt_name);
}

UAVDatatypeHash UAVNode::datatypehash_P(PGM_P name) {
    return datatypehash_P(name, strlen_P((PGM_P)name));
}

UAVDatatypeHash UAVNode::datatypehash(const char *root_ns, const char *subroot_ns, const char *dt_name, uint8_t version) {
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

void UAVNode::add(UAVTransport *transport) {
    _transports.push_back(transport);
    transport->start(*this);
}

void UAVNode::remove(UAVTransport *transport) {
    auto it = std::find(_transports.begin(), _transports.end(), transport);
    if(it==_transports.end()) return;
    _transports.erase(it);
    transport->stop(*this);
}

void UAVNode::port_update(UAVPortID port_id, UAVNodePortInfo* port_info) {
    // notify the transports
    for(auto t : _transports) t->port(*this, port_id, port_info);
}

void UAVNode::add(UAVTask *task) {
    _tasks.push_back(task);
    task->start(*this);
}

void UAVNode::remove(UAVTask *task) {
    auto it = std::find(_tasks.begin(), _tasks.end(), task);
    if(it==_tasks.end()) return;
    _tasks.erase(it);
    task->stop(*this);
}

UAVTransferID UAVNode::next_subject_tid(UAVPortID port) {
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

UAVTransferID UAVNode::next_session_tid(UAVPortID port, UAVNodeID node_id) {
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

void UAVNode::debug_transfer(UAVTransfer *transfer) {
    Serial.print("transfer {");
    Serial.print(" #"); if(transfer->remote_node_id==0xFFFF) { Serial.print(""); } else { Serial.print(transfer->remote_node_id); }
    Serial.print("->#"); if(transfer->local_node_id==0xFFFF) { Serial.print(""); } else { Serial.print(transfer->local_node_id); }
    Serial.print(" "); 
    Serial.print(transfer->port_id & 0x7FFF);
    switch(transfer->transfer_kind) {
        case UAVTransfer::KindMessage: Serial.print(":message."); break;
        case UAVTransfer::KindRequest: Serial.print(":request."); break;
        case UAVTransfer::KindResponse: Serial.print(":response."); break;
    }
    uint64_t tid = transfer->transfer_id;
    Serial.print((uint32_t)(tid>>32),16); Serial.print((uint32_t)(tid),16);
    Serial.print(" "); 
    // port info
    UAVPortInfo * port = ports.list[transfer->port_id];
    if(port==nullptr) {
        Serial.print((uint32_t)(transfer->datatype>>32),16); Serial.print((uint32_t)(transfer->datatype),16);
    } else {
        Serial.print(FPSTR(port->dtf_name));
    }
    // Serial.print(" "); Serial.print(transfer->transfer_kind);
    // uint64_t tid = transfer->transfer_id;
    // Serial.print((uint32_t)(tid>>32),16); Serial.print((uint32_t)(tid),16);
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

void UAVNode::transfer_receive(UAVTransfer *transfer) {
    // debug_transfer(transfer);
    // wrap an input stream around the transfer buffer
    UAVInStream in(transfer->payload, transfer->payload_size);
    // was this a subject broadcast?
    if(transfer->transfer_kind == UAVTransfer::KindMessage) {
        // check the port/datatype combined index
        auto key = std::make_tuple(transfer->port_id, transfer->datatype);
        auto fn = _subscribe_portdata[key];
        if(fn!=nullptr) fn(transfer->remote_node_id, in);
        // all done
        return;
    }
    // was it sent specifically to us?
    if(transfer->local_node_id==local_node_id) {
        if(transfer->transfer_kind == UAVTransfer::KindRequest) {
            // do we have port functions waiting for this?
            UAVNodePortInfo * port = ports.list[transfer->port_id | 0x8000];
            if(port!=nullptr) {
                // create a reply handler for use by the functions.
                bool reply_called = false;
                UAVPortReply reply = [transfer,this,&reply_called](UAVOutStream& out)->void {
                    reply_called = true;
                    respond(
                        transfer->remote_node_id, transfer->port_id, 
                        transfer->transfer_id, transfer->datatype, 
                        transfer->priority, 
                        out.output_buffer, out.output_index
                    );
                };
                // give any port request handlers the chance to reply. first one wins.
                for(auto fn : port->on_request) {
                    fn(*this, in, reply);
                    if(reply_called) break;
                }
            }
        }
        if(transfer->transfer_kind == UAVTransfer::KindResponse) {
            // look up the request index
            auto it = _requests_inflight.find( std::make_tuple(transfer->port_id, transfer->transfer_id) );
            if(it==_requests_inflight.end()) {
                // either the request was already fulfilled and this is a duplicate response, (perhaps via redundant transports)
                // the request never existed, or we've rebooted in the time it took to return.
                // more or less expected behavior now, though we could log 'duplicates' if we cared.
            } else {
                // remember the callback function
                auto fn = it->second;
                // erase it from the index in case things go wrong later.
                _requests_inflight.erase(it);
                // make the call, we are done
                return fn(in);
            } 
        }
    }
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
                e->second(*((UAVInStream *)nullptr));
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
    for(auto transport : _transports) transport->loop(*this,t,dt);
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
                task->loop(*this,t,_task_timer);
            }
            _task_timer = 0;
        }
    }
}

void UAVNode::publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, std::function<void()> callback) {
    return publish(subject_id, datatype, priority, out.output_buffer, out.output_index, callback);
}

void UAVNode::publish(UAVPortID subject_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, std::function<void()> callback) {
    // create a serial transfer header - implicit ref() increment
    auto transfer = new UAVTransfer();
    transfer->on_complete = callback;
    // transfer header
    transfer->timestamp_usec = 0;
    transfer->priority = priority;
    transfer->transfer_kind = UAVTransfer::KindMessage;
    transfer->port_id = subject_id;
    transfer->datatype = datatype;
    transfer->local_node_id = local_node_id;
    transfer->remote_node_id = 0xFFFF; // anonymous id
    transfer->transfer_id = next_subject_tid(subject_id);
    // raw temporary payload
    transfer->payload = payload;
    transfer->payload_size = size;
    // send it via each transport
    for(auto t : _transports) t->send(transfer);
    // release our usage, which might complete the transfer
    transfer->unref();
}

void UAVNode::request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, UAVOutStream& out, UAVPortRequest callback) {
    return request(node_id, service_id, datatype, priority, out.output_buffer, out.output_index, callback);
}

void UAVNode::request(UAVNodeID node_id, UAVPortID service_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, int size, UAVPortRequest callback) {
    UAVPortID port_id = service_id | 0x8000;
    // create a serial transfer
    auto transfer = new UAVTransfer();
    transfer->on_complete = nullptr;
    // transfer header
    transfer->timestamp_usec = 0;
    transfer->priority = priority;
    transfer->transfer_kind = UAVTransfer::KindRequest;
    transfer->port_id = port_id;
    transfer->datatype = datatype;
    transfer->local_node_id = local_node_id;
    transfer->remote_node_id = node_id;
    transfer->transfer_id = next_session_tid(port_id, node_id);
    // payload
    transfer->payload_size = size;
    transfer->payload = payload;
    // put the callback into the requests index
    auto key = std::make_tuple(port_id, transfer->transfer_id);
    _requests_inflight[key] = callback;
    // put an entry into the timeout list
    uint32_t timeout_delay = 2000;
    uint32_t timeout_ms = millis() + timeout_delay;
    _requests_timeout.insert(std::make_pair(timeout_ms, key));
    // send it via each transport
    for(auto t : _transports) t->send(transfer);  
    // release our usage, which might complete the transfer
    transfer->unref();
}

void UAVNode::respond(UAVNodeID node_id, UAVPortID service_id, UAVTransferID transfer_id, UAVDatatypeHash datatype, UAVPriority priority, uint8_t* payload, size_t size) {
    // create a serial transfer
    auto transfer = new UAVTransfer();
    transfer->on_complete = nullptr;
    // transfer header
    transfer->timestamp_usec = 0;
    transfer->priority = priority;
    transfer->transfer_kind = UAVTransfer::KindResponse;
    transfer->port_id = service_id | 0x8000;
    transfer->datatype = datatype;
    transfer->local_node_id = local_node_id;
    transfer->remote_node_id = node_id;
    transfer->transfer_id = transfer_id;
    // payload
    transfer->payload_size = size;
    transfer->payload = payload;
    // send it via each transport
    for(auto t : _transports) t->send(transfer);  
    // release our usage, which might complete the transfer
    transfer->unref();
}

