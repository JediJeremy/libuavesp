#ifndef UV_TRANSPORT_H_INCLUDED
#define UV_TRANSPORT_H_INCLUDED

#include "uv_common.h"
#include <vector>
#include <map>

// common transport property types
using UAVMicrosecond    = uint64_t;
using UAVPriority       = uint8_t;
using UAVTransferKind   = uint8_t;
using UAVPortID         = uint16_t;
using UAVNodeID         = uint16_t;
using UAVTransferID     = uint64_t;
using UAVDatatypeHash   = uint64_t;

// generic transfer structure
class UAVTransfer;
class UAVNodePortInfo;

// abstract interface for transports
class UAVTransport {
    public:
        virtual bool start(UAVNode& node) { return true; }
        virtual void port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) { }
        virtual bool stop(UAVNode& node) { return true; }
        virtual void loop(UAVNode& node, const unsigned long t, const int dt) { }
        virtual void send(UAVTransfer* transfer) { }

        // little-endian integer encoding into transfer buffers - deprecated
        static void encode_uint16(uint8_t *buffer, uint16_t v);
        static void encode_uint32(uint8_t *buffer, uint32_t v);
        static void encode_uint64(uint8_t *buffer, uint64_t v);
        static uint16_t decode_uint16(uint8_t *buffer);
        static uint32_t decode_uint32(uint8_t *buffer);
        static uint64_t decode_uint64(uint8_t *buffer);
};

/// A UAVCAN transfer model (either incoming or outgoing).
/// Per Specification, a transfer is represented on the wire as a non-empty set of transport frames
/// The library is responsible for serializing transfers into transport frames when transmitting, and reassembling
/// transfers from an incoming stream of frames during reception.
class UAVTransfer {
    public:
        // transfer header
        UAVMicrosecond      timestamp_usec;
        UAVPriority         priority;
        UAVTransferKind     transfer_kind;
        UAVPortID           port_id;
        UAVDatatypeHash     datatype;
        UAVNodeID           local_node_id;
        UAVNodeID           remote_node_id;
        UAVTransferID       transfer_id;
        size_t              payload_size;
        uint8_t*            payload;
        // encoded serial frame
        int                 frame_size = 0;
        uint8_t*            frame_data = nullptr;
        // all transfers complete callback
        std::function<void()> on_complete;
        // reference counter
        int ref_count = 1;
        void ref(); 
        void unref(); 
        // virtual destructor
        virtual ~UAVTransfer();
};


// abstract interface for serial transports
class UAVSerialTransport : public UAVTransport {
    public:
        // void send(UAVTransfer* transfer) override;
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

// binary transport streams
class UAVInStream {
    public:
        uint8_t* input_buffer;
        int input_size;
        int input_index;
        int input_remain;
        UAVInStream(uint8_t* buffer, int size) {
            input_buffer = buffer;
            input_size = size;
            input_index = 0;
            input_remain = size;
        }
        void input_memcpy(void* payload, int length);
        friend UAVInStream& operator>>(UAVInStream& s, int8_t& v) { s.input_memcpy((void *)&v,1); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int16_t& v) { s.input_memcpy((void *)&v,2); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int32_t& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int64_t& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint8_t& v) { s.input_memcpy((void *)&v,1); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint16_t& v) { s.input_memcpy((void *)&v,2); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint32_t& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint64_t& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, float& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, double& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, std::string& v) { 
            uint8_t c;
            s.input_memcpy((void *)&c,1);
            v.resize(c);
            s.input_memcpy((void *)v.data(),(int)c); return s; 
        }
};

class UAVOutStream {
    public:
        uint8_t* output_buffer;
        int output_size;
        int output_index;
        int output_remain;
        UAVOutStream(uint8_t* buffer, int size) {
            output_buffer = buffer;
            output_size = size;
            output_index = 0;
            output_remain = size;
        }
        void output_memcpy(const void* payload, int length);
        void output_memcpy_P(PGM_P payload, int length);
        UAVOutStream& P(PGM_P text);
        UAVOutStream& P1(PGM_P text, int limit);
        UAVOutStream& P2(PGM_P text, int limit);
        friend UAVOutStream& operator<<(UAVOutStream& s, const int8_t& v) { s.output_memcpy((void *)&v,1); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int16_t& v) { s.output_memcpy((void *)&v,2); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int32_t& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int64_t& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint8_t& v) { s.output_memcpy((void *)&v,1); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint16_t& v) { s.output_memcpy((void *)&v,2); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint32_t& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint64_t& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const float& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const double& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const char v[]) {
            uint8_t c = min(0xFF, (int)strlen(v));
            s.output_memcpy((void *)&c,1);
            s.output_memcpy((void *)v,(int)c);
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const std::string& v) { /// remove 'const' for greatest number of errors!
            auto len = v.size();
            uint8_t c = min(0xFF, (int)len);
            s.output_memcpy((void *)&c,1);
            s.output_memcpy((void *)v.data(),(int)c);
            return s; 
        }
        // friend UAVOutStream& operator<<(UAVOutStream& s, const PGM_P& v) { s.output_memcpy_P(v,strlen_P(v)); return s; }
};

#endif