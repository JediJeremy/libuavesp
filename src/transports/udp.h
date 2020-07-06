#ifndef LIBUAVESP_TRANSPORT_UDP_H_INCLUDED
#define LIBUAVESP_TRANSPORT_UDP_H_INCLUDED

#include "common.h"
#include "transport.h"
#include "serial.h"
#include "numbermap.h"
#include <ESP8266WiFi.h>

#include "lwip/opt.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "lwip/igmp.h"
#include "lwip/mem.h"

/*
  UDPTransport abstract interface
  This base class provides a few shared capabilities, such as sending arbitrary datagrams
*/ 
class UDPTransport : public UAVTransport {
    protected:
        // lwip port control block
        udp_pcb* _pcb;
        // udp methods
        static void decode_frame(UAVNode& node, UAVNodeID src_node_id, UAVNodeID dst_node_id, uint16_t udp_port, UAVInStream& in);
        ip_addr_t node_addr(UAVNodeID node_id);
    public:
        uint32_t local_ip;
        uint32_t subnet_ip;
        uint32_t subnet_mask;
        uint32_t broadcast_ip;
        // serial transport methods
        bool start(UAVNode& node) override;
        bool stop(UAVNode& node) override;
        void loop(UAVNode& node, const unsigned long t, const int dt) { };
        void send(UAVTransfer* transfer) override;
};

/*
    The PortUDPTransport concrete implementation maintains a map of 'standard' lwip ports to listen on
    which all call a single datagram recieve function. 
*/
class PortUDPTransport : public UDPTransport {
    protected:
        std::map< uint16_t, udp_pcb *> listeners;
        static void udp_recv_fn(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
    public:
        // serial transport methods
        bool start(UAVNode& node) override;
        void port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) override;
        bool stop(UAVNode& node) override;
};

/*
    Listening to all packets isntead of a whitelist
    Not fully implemented yet.
*/
class PromiscousUDPTransport : public UDPTransport {
    protected:
        
    public:
        // serial transport methods
        bool start(UAVNode& node) override;
        bool stop(UAVNode& node) override;
};

#endif