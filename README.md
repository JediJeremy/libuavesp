
     __   __   _______   __   __   _______   _______   __   __
    |  | |  | /   _   \ |  | |  | /   ____| /   _   \ |  \ |  |
    |  | |  | |  |_|  | |  | |  | |  |      |  |_|  | |   \|  |
    |  |_|  | |   _   | \  \_/  / |  |____  |   _   | |  |\   |
    \_______/ |__| |__|  \_____/  \_______| |__| |__| |__| \__|
        |      |            |         |      |         |
    ----o------o------------o---------o------o---------o-------
Uncomplicated Application-level Vehicular Communication And Networking. 

# libuavesp [![Build Status](https://travis-ci.org/JediJeremy/libuavesp.svg?branch=master)](https://travis-ci.org/JediJeremy/libuavesp)
UAVCAN library for Espressif microcontrollers. (Arduino SDK)


### Library Features
* Full rewrite for ESP8266 (soon ESP32)
* Transports over 
  * WiFi UDP Broadcast
  * Serial Hardware UART
  * Serial TCP/IP tunnel
  * Serial Loopback
  * Debug interface
* Application-level server and client functions for:
  * Heartbeat
  * NodeInfo
  * PortInfo
* Extensible framework makes it trivial to add new functions.

### UAVCAN Features:
* Democratic network – no bus master, no single point of failure.
* Publish/subscribe and request/response (RPC) exchange semantics.
* Efficient exchange of large data structures with automatic decomposition and reassembly.
* Lightweight, deterministic, easy to implement, and easy to validate.
* Suitable for deeply embedded, resource constrained, hard real-time systems.
* Specialized interface description language provides rich zero-cost data type and interface abstractions.
* Usable with different transport protocols.
* Supports dual and triply modular redundant transports.
* Supports high-precision network-wide time synchronization.
* High-quality open source reference implementations are freely available under the MIT license.

For more information on the UAVCAN protocol, visit the site:
https://uavcan.org/

For the difference between this and the other UAVCAN projects, imagine Pavel is sitting quietly in an office
carefully designing UAVCAN/canard so that you can build safety-critical autonomous plane networks that won't ever crash,
and meanwhile I've duct-taped myself to the outside of the plane with a spanner, soldering iron and compiler toolkit
shouting "Wooo! Let's get this thing in the air and fix it as we go!" - @JediJeremy

## Overview
* Define datatypes for the messages you will be exchanging.
* Create a UAVNode as the 'virtual interface' for your device.
* Attach network transports to the node so it can communicate with others.
* Attach functions to the node to subscribe to subjects and respond to requests.
* Publish subject messages and make remote requests through the node


## Usage

Create a UAVNode object in the usual way. Nodes start 'anonymous' and should eventually be given an id. 
If the node is going to be associated with TCP or UDP then use the WiFi address to set the id, since it is
required to match.
```C++
UAVNode* uav_node = new UAVNode();
uav_node->set_ip(WiFi);
```

Transports and Apps can be attached to the node. It is expected that transports using local hardware should be set up first,
so that when apps are created they have the chance to send out startup messages. 

Using an Arduino hardware serial port requires wrapping the port (Serial or Serial1) with a HardwareSerialPort object, 
then starting a SerialTransport around the wrapped port, then adding the transport to the node. 
```C++
// UAVCAN Node
UAVNode* uav_node = new UAVNode();

void setup() {
  // add serial transport to the node
  uav_node->add( new SerialTransport( new HardwareSerialPort(Serial) ) );
```

Two optional parameters can be set on many constructors that take a port:
- an owner flag which sets if the port will be auto-destroyed with the transport. Defaults to true for ports passed by pointer.
- "out of band" handler function. Anything that doesn't look like protocol data - such as humans typing on terminals - will be sent to the OOB handler.

## Serial Transports

The SerialTransport is a high level protocol object, it is the SerialPort object which is extended
to create new kinds of serial connections or transcoders. 

eg. To hex-dump frames on serial port 0, use this instead:
```C++
  // wrap hardware serial port in a debug port
  uav_node->add( new SerialTransport( new DebugSerialPort(new HardwareSerialPort(Serial)) ) );
```

One of the simplest serial interfaces is the loopback interface, used mostly for debugging or self-testing.
```C++
  // add a loopback serial transport for testing
  uav_node->add( new SerialTransport( new LoopbackSerialPort() ) );
```

## UDP and TCP Transports

The UDP transport is commonly used. You just need to create one of the concrete transports and add it to the node. 
The WiFi object must be already connected and have an IP address assigned.
```C++
  // start the UDP transport for the node, since we have network
  uav_node->add( new PortUDPTransport(66) );
```

TCP servers act like bundles of serial connections. New serial transports are automatically added and removed
from a node just as if they were hardware ports. You can also indicate if you want the connection wrapped in a debug port.

For each TCP server you also decide whether to provide a _shared UAVNode_ object which will broadcast to all the TCP connections, 
or provide a _factory function_ which can create a _unique UAVNode_ for each new TCP/IP connection. Both are useful in different cases 
and there are big semantic differences between them; memory, security and reliability implications.

This example creates two seperate TCP servers on top of the same node, one normal (on port 66) and the other for hex debugging. (on port 67)
The WiFi must be connected before trying this.
```C++
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,uav_node,false,nullptr);
  // start a debug tcp server over the node on port 67
  tcp_debug = new TCPNode(67,uav_node,true,nullptr);
}
```
It is entirely appropriate for a device to run multiple UAVNode interface objects for different hardware interfaces. 
For example, every transport attached to a node recieves a redundant copy of every message, including replies to
requests that did not originate from that transport. This has security implications for private data.

Using a factory function flips the relationship - now the TCP server is more persistent than UAVNodes. Note that
creating nodes usually means installing 'apps' on them to be useful, which are covered later.
```C++
  // start a tcp server that creates unique nodes on port 68
  tcp_node = new TCPNode(68,[]{ 
    UAVNode* new_node = new UAVNode();
    HeartbeatApp::app_v1(new_node);
    NodeinfoApp::app_v1(new_node);
    return new_node 
  });
}
```
These nodes are not set up with any other transports... no hardware ports are assigned to these 'temporary' nodes. 
(You would expect they'd already been assigned to another.)


## Node Loop

The UAVNode and TCPNode objects need to be polled during the Arduino Loop function, preferably at a high rate to handle
all the network traffic.

```C++
void loop() {
  // [ update clocks and times ]
  // ...
  // UAVCAN node tasks
  uav_node->loop(t,dt);
  // TCP server tasks
  tcp_node->loop(t,dt);
  tcp_debug->loop(t,dt);
  // do OS tasks
  delay(1);
}
```

## Subscriptions

Attaching transports to an otherwise empty node doesn't create interesting behavior. 
A network application consists of messages exchanged between ports, defined as subjects and services. 

We can listen for specific message types being broadcast on the network. This example will print out
every heartbeat recieved by the node:
```C++
  // attach a listener for heartbeat messages
  uav_node->subscribe(
    subjectid_uavcan_node_Heartbeat_1_0,  // integer port number
    dtname_uavcan_node_Heartbeat_1_0,     // PROGMEM flash string containing full canonical datatype name and version
    [](UAVNodeID node_id, UAVInStream& in) {
      HeartbeatMessage m;
      in >> m;
      Serial.print("heartbeat detected {");
      Serial.print(" node:#"); Serial.print(node_id);
      Serial.print(" uptime:"); Serial.print(m.uptime);
      Serial.print("s health:"); Serial.print(m.health);
      Serial.print(" mode:"); Serial.print(m.mode); 
      Serial.print(" vendor:"); Serial.print(m.vendor,16);
      Serial.println(" }");
    }
  );
```

## Publishing Subjects

During app setup it is considered good form to let the node know that you plan to publish subjects,
even if you never actually do. There are lists that it keeps, in case anyone asks.
```C++ 
  // we will be sending messages
  node->define_subject( 
    subjectid_uavcan_node_Heartbeat_1_0,  // integer port number
    dtname_uavcan_node_Heartbeat_1_0      // PROGMEM flash string containing full canonical datatype name and version
  );
```


Sending subject messages is typically done by application 'wrapper' functions and not directly, but looks like so:
```C++ 
  void NewHeartbeatApp::send(UAVNode& node) {
    // update the heartbeat datatype object
    _heartbeat.uptime = millis() / (unsigned long)1000;
    // serialize the object to a stream
    uint8_t payload[16];
    UAVOutStream stream(payload,16);
    stream << _heartbeat;
    // send the heartbeat message
    node.publish(
      subjectid_uavcan_node_Heartbeat_1_0,  // integer port number
      dthash_uavcan_node_Heartbeat_1_0,     // note dthash rather than dtname, precomputed for speed.
      UAVTransfer::PriorityNominal, 
      stream,
      []() { Serial.println("heartbeat sent!"); } // called on completion
    );
}
```
This example has a persistent object called '_heartbeat' that it updates before it serializes to a stream buffer, 
then broadcasts that buffer out via the node. 

The node.publish() function can be given an output stream or byte block. While the pattern of fill-object/serialize-buffer
is expected to be very common, some apps might gain speed from tricks like rewriting a single transmit buffer. Either is fine.

The 'on completion' function is rarely needed. In most cases you won't care if the message was queued and discarded, 
but if you are doing specific rate-limiting or frame timing or counting you might need to know. In this example we log it for fun.


## Services

We can attach service functions to the node. The node will start accepting requests of this datatype, 
and route the request to our function. Inline lambda functions or static object functions work equally well 
so long as they have the correct (*)(node,in,reply) signature - I think the lambda style is very readable.

Typical behavior is to decode the incoming data stream into a datatype object (which handles all the byte parsing)
then do some side-effect, create a return datatype object, and serialize that into a buffer for the reply.
```C++
  uav_node->define_service( 
    portid_ServiceName_1_0, // integer port number
    dtname_ServiceName_1_0, // PROGMEM flash string containing full canonical datatype name and version
    true,                   // do we reply
    [](UAVNode& node, UAVInStream& in, UAVPortReply reply) {
      // decode the request using a datatype object
      ServiceRequest q;
      in >> q;
      // compose a reply using a datatype object
      ServiceReply r;
      // [ this is where some magic would happen ]
      // serialize the reply into a fixed-sized buffer. how big? you should know.
      uint8_t buffer[128];
      UAVOutStream out(buffer,128);
      out << r;
      // reply with the filled stream.
      reply(out);
    }
  );
```

Services need to run fast; calls should finish within the millisecond. No waiting around, you're on a timer at both ends. 
If you can't immediately reply perhaps rethink your call pattern so you can publish a subject message later when the data 
is available.

It is a feature of UAVCAN datatypes that you're supposed to have a very good idea of how big a buffer you'll need for each
message you expect to send. In hard real-time systems there will be pages of pre-allocated buffers waiting in a queue.

If you have written a proper datatype specification, the DSDL compiler will tell you this size. In practise you can eyeball 
the data structure, count the bytes, go "eh, round it up by a power of two-ish" and it's your own fault if you undercounted. 
The output stream knows when to stop, at least. 

The code style seems a little clunky, but it's making use of compiler conventions which vastly speed up object allocation.
Buffers and streams that are declared together within a function are cheap, so long as you don't need them to last beyond the current scope.
If you try wrapping that minimum amount of code in another function, you're leaving scope by definition.

### Built-in Apps

There are reference implementations provided for several apps defined by the specification. Each app has 'installer'
functions to set up a UAVNode. Some apps require extra parameters to connect them with other system or setup objects. 

Apps can add timed tasks to the node (like Heartbeat) which send regular subject broadcasts.
```C++
  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
  RegisterApp::app_v1(uav_node, system_registers);
```

App classes can provide API methods which are typically named for remote UAVCAN services they call. 

eg: The NodeInfo API has a wrapper for the ExecuteCommand() service call, which sends a number
code and a short string as part of the request. The remote node will send a status reply. This 
wrapper function does all the stream-to-object conversion in both directions for us.
```C++
  // send an order to ourselves
  NodeinfoApp::ExecuteCommand(
    uav_node, // send from this node instance
    node_id,  // send to this node ID
    66, "Execute order 66!", // command properties
    [](NodeExecuteCommandReply* reply) { // callback function
      if(reply==nullptr) {
        Serial.println("command result {}");
      } else {
        Serial.println("command result {");
        Serial.print(" status: "); Serial.print(reply->status); Serial.println();
        Serial.println("}");
      }
    }
  );
```

Note that service callbacks will always return eventually, thanks to timeouts, if the underlying process fails.
A null reply object is provided in those error/timeout cases. The callback function will always be called once,
and only once, once the result is known.

This is what makes the API functions useful - they handle all the messy buffer/stream details
and give us back a fully parsed result datatype object. (or nothing at all) The request object
serialization is also hidden and parameters are sanity checked. 


## Examples
* HeartbeatListener.ino subscribes to heartbeat messages (including it's own) on a variety of network transports and logs them to the serial port.
* SerialOOB.ino shows how to attach "out of band" functions when setting up serial transports, in case you expect a human to also be on the line
* Order66.ino Tests Node.GetInfo and Node.ExecuteCommand and will occasionally command the execution of order 66 on nearby nodes.
