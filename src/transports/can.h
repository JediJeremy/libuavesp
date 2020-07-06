#ifndef LIBUAVESP_TRANSPORT_CANARD_H_INCLUDED
#define LIBUAVESP_TRANSPORT_CANARD_H_INCLUDED

#include "../common.h"
#include "../node.h"
#include "../transport.h"

// #include <canard.h>
#ifdef CANARD_H_INCLUDED

/*
  CanardTransport abstract interface
  Wrapper for libcanard to work as a transport under libuavesp
*/ 
class CanardTransport : public UAVTransport {
    protected:
        // canard instance
        CanardInstance _canard;
        
        // memory allocator for canard
        static void* canard_allocate(CanardInstance* ins, size_t amount);
        static void canard_free(CanardInstance* ins, void* pointer);

    public:
        // serial transport methods
        bool start(UAVNode& node) override;
        void port(UAVNode& node, UAVPortID port_id, UAVNodePortInfo* info) override;
        bool stop(UAVNode& node) override;
        void loop(UAVNode& node, const unsigned long t, const int dt) { };
        void send(UAVTransfer* transfer) override;
};
#endif
#endif
