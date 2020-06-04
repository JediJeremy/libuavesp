#ifndef UV_TRANSPORT_TCP_H_INCLUDED
#define UV_TRANSPORT_TCP_H_INCLUDED

#include "uv_common.h"
#include "uv_transport.h"
#include "uv_transport_serial.h"
#include "numbermap.h"
#include <ESP8266WiFi.h>
#include <vector>

#define MAX_TCP_CLIENTS 10

class TCPSerialPort : public UAVSerialPort {
    protected:
        WiFiClient * _client;
    public:
        TCPSerialPort(WiFiClient* client);
        ~TCPSerialPort();
        void read(uint8_t *buffer, int count) override;
        void write(uint8_t *buffer, int count) override;
        void flush() override;
        int readCount() override;
        int writeCount() override;
};

class TCPSerialTransport : public SerialTransport {
    protected:
        WiFiClient * _client;
    public:
        TCPSerialTransport (WiFiClient* client, UAVSerialPort *port, bool owner, SerialOOBHandler oob);
        virtual ~TCPSerialTransport();
        void close();
        bool closed();
        // void loop(const unsigned long t, const int dt, UAVNode* node);
};

class TCPNode {
    protected:
        UAVNode * _node;
        WiFiServer *_server;
        std::vector<TCPSerialTransport *>_clients;
        int _client_count = 0;
        bool _debug;
    public:
        // out-of-band handler for client transports
        SerialOOBHandler oob_handler = nullptr;
        // con/destructors
        TCPNode (int server_port, UAVNode * node, bool debug, SerialOOBHandler oob);
        virtual ~TCPNode();
        // tcp server interface
        bool start();
        bool stop();
        void loop(const unsigned long t, const int dt);
};


#endif