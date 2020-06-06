#ifndef UV_TRANSPORT_SERIAL_H_INCLUDED
#define UV_TRANSPORT_SERIAL_H_INCLUDED

#include "uv_common.h"
#include "uv_node.h"
#include "numbermap.h"

#define UV_SERIAL_FRAME_VERSION_0   0x00
#define UV_SERIAL_FRAME_DELIMITER   0x9E
#define UV_SERIAL_ESCAPE_PREFIX     0x8E
#define UV_SERIAL_NODEID_MASK       0x0FFF
     

#define UV_SERIAL_RX_STATE_NONE      0
#define UV_SERIAL_RX_STATE_OOB       1
#define UV_SERIAL_RX_STATE_DELIMITER 2
#define UV_SERIAL_RX_STATE_FRAME     3
#define UV_SERIAL_RX_STATE_ESCAPE    4

#define UV_SERIAL_MAX_FRAME_SIZE           1024
#define UV_SERIAL_CRC_SIZE                 4
#define UV_SERIAL_HEADER_WITHOUT_CRC_SIZE  28
#define UV_SERIAL_HEADER_WITH_CRC_SIZE     (UV_SERIAL_HEADER_WITHOUT_CRC_SIZE + UV_SERIAL_CRC_SIZE)
#define UV_SERIAL_MIN_FRAME_SIZE           (UV_SERIAL_HEADER_WITH_CRC_SIZE + UV_SERIAL_CRC_SIZE)

#define UV_SERIAL_DEBUG_LINE 16


class HardwareSerialPort : public UAVSerialPort {
    protected:
        HardwareSerial * _port;
    public:
        HardwareSerialPort(HardwareSerial& port);
        ~HardwareSerialPort();
        void read(uint8_t *buffer, int count) override;
        void write(uint8_t *buffer, int count) override;
        void flush() override;
        int readCount() override;
        int writeCount() override;
};

class LoopbackSerialPort : public UAVSerialPort {
    protected:
        uint8_t _buffer[128];
        int _size=128;
        int _head=0;
        int _tail=0;
        int _count=0;
    public:
        LoopbackSerialPort();
        ~LoopbackSerialPort();
        void read(uint8_t *buffer, int count) override;
        void write(uint8_t *buffer, int count) override;
        void flush() override;
        int readCount() override;
        int writeCount() override;
};


class DebugSerialPort : public UAVSerialPort {
    protected:
        UAVSerialPort * _port;
        bool            _owner;
        uint8_t         _line[UV_SERIAL_DEBUG_LINE];
        int             _index = 0;
        int             _flushed = 0;
        int             _mode = 0;
        void   write_hex(uint8_t c);
        int   write_line(int wc);
        void end_of_line(int mode);
    public:
        DebugSerialPort(UAVSerialPort& port);
        DebugSerialPort(UAVSerialPort* port, bool owner);
        ~DebugSerialPort();
        void read(uint8_t *buffer, int count) override;
        void write(uint8_t *buffer, int count) override;
        void flush() override;
        int readCount() override;
        int writeCount() override;
};

typedef struct {
    SerialTransfer* transfer;
    int             transfer_state;
    int             frame_size;
    int             frame_index;
    uint8_t*        frame_buffer;
} SerialFrame;

using SerialOOBHandler = void (*) (UAVTransport *transport, SerialFrame* rx, uint8_t* buffer, int count);

class SerialInterface {
    protected:
    public:
};

// concrete serial transport
class SerialTransport : public UAVSerialTransport {
    protected:
        UAVSerialPort*  _port;
        bool            _owner;
        SerialFrame*    _rx;
        SerialFrame*    _tx;
        NumberMap  *    _queue;
    public:
        // out-of-band handler
        SerialOOBHandler oob_handler = nullptr;
        // con/destructors
        SerialTransport(UAVSerialPort* port, bool owner, SerialOOBHandler oob);
        SerialTransport(UAVSerialPort& port) : SerialTransport{&port,false,nullptr} {};
        virtual ~SerialTransport();
        // serial transport methods
        bool start();
        bool stop();
        void loop(const unsigned long t, const int dt, UAVNode* node) override;
        void send(SerialTransfer* transfer) override;
        // frame encoding and decoding
        static void encode_frame(SerialTransfer* transfer, UAVNode *node);
        static bool decode_frame(SerialFrame* rx, UAVNode *node);
        void parse_buffer(uint8_t* parse, int count, UAVNode* node);
        // transmit queue management
        void dequeue(int index);
        static void transfer_complete(SerialTransfer* transfer);
};


#endif