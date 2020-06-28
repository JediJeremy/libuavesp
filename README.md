
     __   __   _______   __   __   _______   _______   __   __
    |  | |  | /   _   \ |  | |  | /   ____| /   _   \ |  \ |  |
    |  | |  | |  |_|  | |  | |  | |  |      |  |_|  | |   \|  |
    |  |_|  | |   _   | \  \_/  / |  |____  |   _   | |  |\   |
    \_______/ |__| |__|  \_____/  \_______| |__| |__| |__| \__|
        |      |            |         |      |         |
    ----o------o------------o---------o------o---------o-------
Uncomplicated Application-level Vehicular Communication And Networking. 

# libuavesp
UAVCAN library for Espressif microcontrollers. (Arduino SDK)

### Library Features
* Full rewrite for ESP8266 (soon ESP32)
* Transports over 
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
UAVNode uav_node();
uav_node.serial_node_id = ip_addr & UV_SERIAL_NODEID_MASK;
```

Transports and Apps can be attached to the node. It is expected that transports using local hardware should be set up first,
so that when apps are created they have the chance to send out 'startup' messages. 

Because transports come in very different 'classes', there are transport-specific management methods on the node object.
eg: Serial transports form a common class which can share internal buffers.

Starting a protocol connection on a hardware serial port requires wrapping the Arduino port (Serial or Serial1)
with a HardwareSerialPort object, then starting a SerialTransport using the SerialPort, then adding the transport to the node. 
You can even recieve "out of band" data - anything that doesn't look like protocol data, like humans typing on terminals - if you
provide an 'oob' handler.

```C++
// UAVCAN Node
UAVNode uav_node();
// provide an 'out-of-band' function
void serial_oob(UAVTransport *transport, SerialFrame* rx, uint8_t* buf, int count) { } 

void setup() {
  // during setup, add serial transport to the node
  uav_node.serial_add( new SerialTransport(new HardwareSerialPort(Serial), true, serial_oob) );
```

Note the owner flag set to 'true' which means we expect the port to be auto-destroyed when the transport goes.

One of the simplest serial interfaces is the loopback interface, used for debugging. 
```C++
  // add a loopback serial transport for testing
  SerialTransport* loopback = new SerialTransport( new LoopbackSerialPort(), true, serial_oob);
  uav_node.serial_add( loopback );
```

The SerialTransport is a generic protocol-level object and it is the SerialPort object which is swapped
to create new kinds of serial transport. It can even be wrapped in another object that presents a SerialPort API while doing some kind of translation/repacking. The debug port 'wrapper' does this to translate binary code into hex-dump format.

eg. To hex-dump frames on serial port 0, use this instead:
```C++
  // wrap hardware serial port in a debug port
  uav_node.serial_add( new SerialTransport(new DebugSerialPort(new HardwareSerialPort(Serial),true), true, serial_oob) );
```

TCP servers act as virtual sets of serial connections. New serial transports are automatically added and removed
from a node just as above. You can indicate if you want the connection wrapped in a debug port for extra fun.

You can choose decide whether you provide a _shared UAVNode_ object which will redundantly broadcast to all the TCP connections, or whether you provide a _factory function_ which will create a _unique UAVNode_ for each TCP/IP connection. There are some large semantic differences between those things, as well as memory differences, and both are useful.

This example will create two seperate TCP servers from the same node, one normal and the other for hex debug
```C++
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,&uav_node,false,nullptr);
  // start a debug tcp server over the node on port 67
  tcp_debug = new TCPNode(67,&uav_node,true,nullptr);
}
```
It is entirely appropriate for a device to run multiple UAVNode interface objects for different hardware interfaces. 

The UAVNode and TCPNode objects need to be polled during the Arduino Loop function, preferably at a high rate to handle
all the network traffic.
```C++
void loop() {
  // [ update clocks and times ]
  // ...
  // UAVCAN node tasks
  uav_node.loop(t,dt);
  // TCP server tasks
  tcp_node->loop(t,dt);
  tcp_debug->loop(t,dt);
  // do OS tasks
  delay(1);
}
```

After all that you have an empty node that doesn't do anything, but which is listening on all kinds of networking interfaces.
We need to install "apps" on the node... groups of functions that send and recieve data frames.
```C++
  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
  RegisterApp::app_v1(uav_node, system_registers);
```

Some app classes (like heartbeat) will add tasks which begin sending regular subject broadcasts.

Most app classes will attach port or subject listeners to the node to recieve wanted messages.

Most app classes provide API methods which are named for remote UAVCAN functions they call. 
They generally call the API and return some kind of cooked reply object via a callback.

```C++
  // send an order to ourselves
  NodeinfoApp::ExecuteCommand(
    uav_node, node_id,
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

We can attach our own service functions to the node, by supplying all the port metadata and a lambda function.
```C++
  uav_node->ports.define_service( 
    portid_ServiceName_1_0, // integer port number
    dtname_ServiceName_1_0, // PROGMEM flash string containing full canonical datatype name and version
    true,                   // do we reply
    [&registers](UAVNode * node, UAVInStream& in, UAVPortReply reply) {
      // decode the request
      ServiceRequest q;
      in >> q;
      // compose a reply
      ServiceReply r;
      // serialize the reply
      uint8_t buffer[128];
      UAVOutStream out(buffer,128);
      out << r;
      reply(out);
    }
  );
```

We can listen for specific message types being broadcast on the network. This example will print out
every heartbeat recieved by the node.
```C++
  // attach a listener for heartbeat messages
  uav_node->subscribe(
    portid_ServiceName_1_0, // integer port number
    dthash_ServiceName_1_0, // 64-bit datatype hash
    [](SerialNodeID node_id, UAVInStream& in) {
      HeartbeatMessage m;
      in >> m;
      Serial.println("heartbeat detected {");
      Serial.print("    node : #"); Serial.print(node_id); Serial.println();
      Serial.print("  uptime : "); Serial.print(m.uptime); Serial.println("s");
      Serial.print("  health : "); Serial.print(m.health); Serial.println();
      Serial.print("    mode : "); Serial.print(m.mode); Serial.println();
      Serial.print("  vendor : "); Serial.print(m.vendor,16); Serial.println();
      Serial.println("}");
    }
  );
```

Apps are merely pre-packaged sets of custom data types and service functions. 




## Examples
* Order66.ino Tests the Node.GetInfo and Node.ExecuteCommand and will occasionally command the execution of order 66.
