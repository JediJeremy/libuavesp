#ifndef UV_TRANSPORT_UDP_H_INCLUDED
#define UV_TRANSPORT_UDP_H_INCLUDED

#include "uv_common.h"
#include "uv_transport.h"
#include "uv_transport_serial.h"
#include "numbermap.h"
#include <ESP8266WiFi.h>
// #include <WiFiUdp.h>
#include "espconn.h"


class UDPTransport : public UAVSerialTransport {
    protected:
        // udp methods
        void decode_frame(UAVNode& node, IPAddress node_ip, uint16_t udp_port, UAVInStream& in);        
    public:
        IPAddress broadcast_ip;
        IPAddress subnet_ip;
        // serial transport methods
        bool start(UAVNode& node) override;
        bool stop(UAVNode& node) override;
        void loop(UAVNode& node, const unsigned long t, const int dt) { };
        void send(SerialTransfer* transfer) override;
};


class PortUDPTransport : public UDPTransport {
    protected:
        std::map< uint16_t, espconn *> listeners;
    public:
        // serial transport methods
        bool start(UAVNode& node) override;
        bool stop(UAVNode& node) override;
};

class PromiscousUDPTransport : public UDPTransport {
    protected:
        
    public:
        // serial transport methods
        bool start(UAVNode& node) override;
        bool stop(UAVNode& node) override;
};

#endif