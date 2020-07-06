#ifndef LIBUAVESP_TRANSPORT_TCP_H_INCLUDED
#define LIBUAVESP_TRANSPORT_TCP_H_INCLUDED

#include "../common.h"
#include "../node.h"
#include "../transport.h"
#include "../numbermap.h"
#include "serial.h"
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
        UAVNode * unique_node = nullptr;
        TCPSerialTransport (WiFiClient* client, UAVSerialPort *port, bool owner, SerialOOBHandler oob);
        virtual ~TCPSerialTransport();
        void close();
        bool closed();
};

class TCPNode {
    protected:
        UAVNode * _node = nullptr;
        std::function<UAVNode*()> _node_fn;
        WiFiServer *_server;
        std::vector<TCPSerialTransport *>_clients;
        int _client_count = 0;
        bool _debug;
    public:
        // out-of-band handler for client transports
        SerialOOBHandler oob_handler = nullptr;
        // con/destructors
        TCPNode (int server_port, UAVNode * node, bool debug, SerialOOBHandler oob);
        TCPNode (int server_port, std::function<UAVNode*()> node_fn, bool debug, SerialOOBHandler oob);
        virtual ~TCPNode();
        // tcp server interface
        bool start();
        bool stop();
        void loop(const unsigned long t, const int dt);
};


#endif