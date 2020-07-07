
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
If the node is going to be associated with TCP or UDP then use the WiFi address to set the id. 
Or ids can be statically configured, or allocated with the PnP protocol.
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
  uav_node->add( new SerialTransport(new HardwareSerialPort(Serial), true, nullptr) );
```

Note the owner flag set to 'true' which means the wrapper port will be auto-destroyed with the transport.

You can even recieve "out of band" data - anything that doesn't look like protocol data, such as humans typing on terminals - 
if you provide an 'oob handler'. 


One of the simplest serial interfaces is the loopback interface, used mostly for debugging. 
```C++
  // add a loopback serial transport for testing
  uav_node->add( new SerialTransport( new LoopbackSerialPort(), true, nullptr) );
```

The SerialTransport is a high level protocol object, it is the SerialPort object which is extended
to create new kinds of serial connections or transcoders. 

eg. To hex-dump frames on serial port 0, use this instead:
```C++
  // wrap hardware serial port in a debug port
  uav_node->add( new SerialTransport(new DebugSerialPort(new HardwareSerialPort(Serial),true), true, nullptr) );
```

The UDP transport is commonly used. You just need to create one of the concrete transports and
add it to the node. 
```C++
  // start the UDP transport for the node, since we have network
  uav_node->add( new PortUDPTransport() );
```

TCP servers act as virtual sets of serial connections. New serial transports are automatically added and removed
from a node just as above. You can indicate if you want the connection wrapped in a debug port for extra fun.

You can decide whether to provide a _shared UAVNode_ object which will redundantly broadcast to all the TCP connections, or whether a _factory function_ which should create a _unique UAVNode_ for each TCP/IP connection. There are large semantic differences between those things, security and reliability implications, memory requirements, and both are useful.

This example will create two seperate TCP servers from the same node, one normal and the other for hex debug.
It's best to wait until the WiFi is connected before trying this.
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

Attaching transports to an otherwise empty node doesn't create interesting behavior. 
A network application consists of messages exchanged between ports, defined as subjects and services. 

We can listen for specific message types being broadcast on the network. This will print out
every heartbeat recieved by the node.
```C++
  // attach a listener for heartbeat messages
  uav_node->subscribe(
    subjectid_uavcan_node_Heartbeat_1_0, 
    dtname_uavcan_node_Heartbeat_1_0, 
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


We can attach our own service functions to the node. This will tell the node to start accepting requests of
this datatype, and route the request to our function. inline lambda functions or static object functions will
work so long as they have the correct (*)(node,in,reply) signature.

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
      // serialize the reply into a fixed-sized buffer
      uint8_t buffer[128];
      UAVOutStream out(buffer,128);
      out << r;
      // reply with the filled stream
      reply(out);
    }
  );
```

Groups of these can be defined and 'installed' on the node seperately. There are several applications
defined by the specification that are provided with reference implementations.

```C++
  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
  RegisterApp::app_v1(uav_node, system_registers);
```

Some apps (like heartbeat) will add tasks which begin sending regular subject broadcasts.

Most app classes provide API methods which are named for remote UAVCAN functions they call. 
They generally call the API and return some kind of reply object via a callback lambda. The 
lambda function will be called back eventually - either with the reply data, or null on timeout.

eg: The NodeInfo API has a wrapper for the ExecuteCommand() service call, which sends a number
code and a short string as part of the request. The remote node will send a status reply.

```C++
  // send an order to ourselves
  NodeinfoApp::ExecuteCommand(
    uav_node, 
    node_id,
    66, "Execute order 66!",
    [](NodeExecuteCommandReply* reply) {
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

## Examples
* HeartbeatListener.ino subscribes to heartbeat messages (including it's own) on a variety of network transports and logs them to the serial port.
* SerialOOB.ino shows how to attach "out of band" functions when setting up serial transports, in case you expect a human to also be on the line
* Order66.ino Tests Node.GetInfo and Node.ExecuteCommand and will occasionally command the execution of order 66 on nearby nodes.
