#ifndef UV_TRANSPORT_H_INCLUDED
#define UV_TRANSPORT_H_INCLUDED

#include "uv_common.h"
#include <vector>
#include <map>

// the enabled transports
// extern uint32_t uv_transports;
#define UV_TRANSPORT_CAN     0x0001
#define UV_TRANSPORT_SERIAL0 0x0002
#define UV_TRANSPORT_SERIAL1 0x0004
#define UV_TRANSPORT_SERIAL2 0x0008
#define UV_TRANSPORT_UDP     0x0010
#define UV_TRANSPORT_TCP     0x0020

// abstract interface for transports
class UAVTransport {
    public:
        virtual bool start();
        virtual bool stop();
        virtual void loop(const unsigned long t, const int dt, UAVNode* node);
        // little-endian integer encoding into transfer buffers
        static void encode_uint16(uint8_t *buffer, uint16_t v);
        static void encode_uint32(uint8_t *buffer, uint32_t v);
        static void encode_uint64(uint8_t *buffer, uint64_t v);
        static uint16_t decode_uint16(uint8_t *buffer);
        static uint32_t decode_uint32(uint8_t *buffer);
        static uint64_t decode_uint64(uint8_t *buffer);
};

// generic serial transport structures
//   used by HardwareSerial and TCPSerial interfaces

//typedef uint16_t SerialNodeID;
//typedef uint64_t SerialTransferID;

using SerialNodeID = uint16_t;
using SerialTransferID = uint64_t;

/// A UAVCAN transfer model (either incoming or outgoing).
/// Per Specification, a transfer is represented on the wire as a non-empty set of transport frames
/// The library is responsible for serializing transfers into transport frames when transmitting, and reassembling
/// transfers from an incoming stream of frames during reception.
class SerialTransfer {
    public:
        // transfer header
        CanardMicrosecond   timestamp_usec;
        CanardPriority      priority;
        CanardTransferKind  transfer_kind;
        CanardPortID        port_id;
        uint64_t            datatype;
        SerialNodeID        local_node_id;
        SerialNodeID        remote_node_id;
        SerialTransferID    transfer_id;
        size_t              payload_size;
        uint8_t*            payload;
        // encoded frame properties
        int                 frame_usage;
        int                 frame_size;
        uint8_t*            frame_data;
        // all transfers complete callback
        std::function<void()> on_complete;
};

// abstract interface for serial transports
class UAVSerialTransport : public UAVTransport {
    public:
        virtual void send(SerialTransfer* transfer);
};

// abstract interface for generic serial ports
// concete examples are hardware UARTs and TCP/IP connections
class UAVSerialPort {
    public:
        virtual void read(uint8_t *buffer, int count);
        virtual void write(uint8_t *buffer, int count);
        virtual void flush();
        virtual int readCount();
        virtual int writeCount();
        void print(char * string);
        void println();
        void println(char * string);
};


#endif