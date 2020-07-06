#include "transport.h"
//#include "transports/canard.h"
//#include "transports/serial.h"
//#include "transports/udp.h"
//#include "transports/tcp.h"
#include "numbermap.h"
#include "crc32c.h"

// UAVTransfer
// reference counter increment
void UAVTransfer::ref() { 
    if(ref_count==0) return;
    ref_count++; 
}
// reference counter decrement
void UAVTransfer::unref() {
    if(ref_count==0) return;
    ref_count--;
    if(ref_count==0) {
        if(on_complete!=nullptr) on_complete();
        delete this;
    }
}
// destructor
UAVTransfer::~UAVTransfer() {
    // assume we are the owner of frame_data
    if(frame_data != nullptr) {
        delete frame_data;
    }
}

// UAVSerialPort
// stream methods
void UAVSerialPort::read(uint8_t *buffer, int count) { }
void UAVSerialPort::write(uint8_t *buffer, int count) { }

void UAVSerialPort::print(char * string) {
    int remain = strlen(string);
    int index = 0;
    while(remain>0) {
        int c = min(remain, writeCount());
        write((uint8_t*)&string[index], c);
        remain -=c;
        index +=c;
        if(remain>0) delay(100);
    }
}

void UAVSerialPort::println() {
    print("\r\n");
}

void UAVSerialPort::println(char * string) {
    print(string);
    print("\r\n");
}


// UAVTransport
// old primitive type encode and decode functions... [deprecated: Use in/out streams]

void UAVTransport::encode_uint16(uint8_t *buffer, uint16_t v) {
    buffer[0] = v;
    buffer[1] = v >> 8;
}

void UAVTransport::encode_uint32(uint8_t *buffer, uint32_t v) {
    buffer[0] = v;
    buffer[1] = v >> 8;
    buffer[2] = v >> 16;
    buffer[3] = v >> 24;
}

void UAVTransport::encode_uint64(uint8_t *buffer, uint64_t v) {
    buffer[0] = v;
    buffer[1] = v >> 8;
    buffer[2] = v >> 16;
    buffer[3] = v >> 24;
    buffer[4] = v >> 32;
    buffer[5] = v >> 40;
    buffer[6] = v >> 48;
    buffer[7] = v >> 56;    
}


uint16_t UAVTransport::decode_uint16(uint8_t *buffer) {
    return ((uint16_t)buffer[0]) 
        + (((uint16_t)buffer[1])<<8);
}

uint32_t UAVTransport::decode_uint32(uint8_t *buffer) {
    return ((uint32_t)buffer[0]) 
        + (((uint32_t)buffer[1])<<8) 
        + (((uint32_t)buffer[2])<<16) 
        + (((uint32_t)buffer[3])<<24);
}

uint64_t UAVTransport::decode_uint64(uint8_t *buffer) {
    return ((uint64_t)buffer[0]) 
        + (((uint64_t)buffer[1])<<8)
        + (((uint64_t)buffer[2])<<16)
        + (((uint64_t)buffer[3])<<24)
        + (((uint64_t)buffer[4])<<32)
        + (((uint64_t)buffer[5])<<40)
        + (((uint64_t)buffer[6])<<48)
        + (((uint64_t)buffer[7])<<56);
}


// UAVInStream
// memory copy methods
void UAVInStream::input_memcpy(void* payload, int length) {
    if(input_remain<length) return;
    memcpy( payload, &input_buffer[input_index], length);
    input_index+=length;
    input_remain-=length;
}


// UAVOutStream
// memory copy methods
void UAVOutStream::output_memcpy(const void* payload, int length) {
    if(output_remain<length) return;
    memcpy( &output_buffer[output_index], payload, length);
    output_index+=length;
    output_remain-=length;
}

void UAVOutStream::output_memcpy_P(PGM_P payload, int length) {
    if(output_remain<length) return;
    memcpy_P( &output_buffer[output_index], payload, length);
    output_index+=length;
    output_remain-=length;
}

UAVOutStream& UAVOutStream::P(PGM_P text) {
    int length = strlen_P(text);
    output_memcpy_P(text,length);
    return *this;
}

UAVOutStream& UAVOutStream::P1(PGM_P text, int limit) {
    int length = min(limit,(int)strlen_P(text));
    uint8_t c = length;
    output_memcpy(&c,1);
    output_memcpy_P(text,length);
    return *this;
}

UAVOutStream& UAVOutStream::P2(PGM_P text, int limit) {
    int length = min(limit,(int)strlen_P(text));
    uint16_t c = length;
    output_memcpy(&c,2);
    output_memcpy_P(text,length);
    return *this;
}

// DatatypeHash test functions
/*
void dthash_parsed_debug(const char *name) {
  uint64_t hash = UAVNode::datatypehash(name);
  unsigned long hash_upper = hash >> 32;
  unsigned long hash_lower = hash;
  Serial.print(name); 
  Serial.print(' '); Serial.print(hash_upper, 16); 
  Serial.print('_'); Serial.print(hash_lower >> 20 & 0xFFF, 16);
  Serial.print('_'); Serial.print(hash_lower >> 8 & 0xFFF, 16);
  Serial.print('_'); Serial.println(hash_lower & 0xFF, 16);
}

void dthash_unparsed_debug(const char *root, const char *subroot, const char *tail, uint8_t version) {
  uint64_t hash = UAVNode::datatypehash(root, subroot, tail, version);
  unsigned long hash_upper = hash >> 32;
  unsigned long hash_lower = hash;
  Serial.print(root); Serial.print('.'); 
  if(subroot!=NULL) { Serial.print(subroot); Serial.print('.'); }
  Serial.print(tail); Serial.print('.'); 
  Serial.print(version); Serial.print('.x'); 
  Serial.print(' '); Serial.print(hash_upper, 16); 
  Serial.print('_'); Serial.print(hash_lower >> 20 & 0xFFF, 16);
  Serial.print('_'); Serial.print(hash_lower >> 8 & 0xFFF, 16);
  Serial.print('_'); Serial.println(hash_lower & 0xFF, 16);
}

void dthash_test() {
  dthash_parsed_debug("uavcan.Test.255.1");
  dthash_parsed_debug("uavcan.internet.udp.OutgoingPacket.0.1");
  dthash_parsed_debug("uavcan.internet.udp.HandleIncomingPacket.0.1");
  dthash_parsed_debug("uavcan.node.Version.1.0");
  dthash_parsed_debug("uavcan.node.GetInfo.0.1");
  dthash_parsed_debug("uavcan.node.GetTransportStatistics.0.1");

  dthash_unparsed_debug("uavcan",NULL,"Test",255);
  dthash_unparsed_debug("uavcan","internet","udp.OutgoingPacket",0);
  dthash_unparsed_debug("uavcan","internet","udp.HandleIncomingPacket",0);
  dthash_unparsed_debug("uavcan","node","Version",1);
  dthash_unparsed_debug("uavcan","node","GetInfo",0);
  dthash_unparsed_debug("uavcan","node","GetTransportStatistics",0);
}
*/