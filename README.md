
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
* Full rewrite for ESP8266 (ESP32 coming soon)
* Extensible framework makes it trivial to add new capabilities.
* Transports over 
  * USB & Serial Hardware
  * UDP Broadcast over WiFi
  * TCP/IP Tunnel over WiFi
  * Loopback & Hex Debugger
* APIs for standard UAVCAN protocols:
  * Heartbeat
  * NodeInfo
  * PortInfo

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

For the difference between this and the other UAVCAN-related projects, imagine Pavel is sitting quietly in an office carefully designing UAVCAN so that you can build safety-critical autonomous plane networks that won't ever crash, and meanwhile I've duct-taped myself to the outside of the test plane with a screwdriver, soldering iron and compiler toolkit shouting "Wooo! Let's get this thing in the air and fix it as we gooo!" - @JediJeremy


## Usage

Create a UAVNode object in the usual way. Nodes start 'anonymous' and should eventually be given an id. 
If the node is going to be associated with TCP or UDP then we must get it from the WiFi address once connected.
```C++
  UAVNode* uav_node = new UAVNode();
  // ... connect WiFi ....
  uav_node->set_id(WiFi);
```

Transports are communications channels between UAVNode objects. Transports over local hardware should be set up first, so that when apps are installed they have the chance to send out startup messages.

Wrappers are provided for the Arduino hardware serial ports. Creating a new transport protocol, its hardware port interface, and adding it to the node can be done in one line:
```C++
  // add serial transport to the node
  uav_node->add( new SerialTransport( new HardwareSerialPort(Serial) ) );
```

Two optional parameters can be set on most constructors that take a port:
- an owner flag which sets if the port will be auto-destroyed with the transport. Defaults to true for ports passed by pointer.
- "out of band" handler function. Anything that doesn't look like protocol data - such as humans typing on terminals, or an entirely different protocol trying to connect - will be sent to the OOB handler.

### Serial Transports

The SerialTransport is a high level protocol object, it is the SerialPort object which is extended
to create new kinds of connections or inline transcoders. 

eg. To hex-dump frames, wrap a 'debug' port around the basic hardware port:
```C++
  // wrap hardware serial port in a debug port
  uav_node->add( new SerialTransport( new DebugSerialPort(new HardwareSerialPort(Serial)) ) );
```

One of the simplest serial interfaces is the loopback interface, used mostly for self-testing. It just
loops the outgoing data back into the input port, so that we hear our own messages like an echo.
```C++
  // add a loopback serial transport for testing
  uav_node->add( new SerialTransport( new LoopbackSerialPort() ) );
```

### UDP and TCP Transports

UDP over WiFi is very useful. So useful there will probably be multiple implementations of the transport.
For now, the WiFi object should be already connected and have an IP address assigned before creating the
transport.
```C++
  // start the UDP transport for the node, since we have network
  uav_node->add( new PortUDPTransport(66) );
```

TCP servers act like bundles of serial connections. New connections are automatically added and removed
from a node just as if they were hardware ports.

For each TCP server you also decide whether to provide a _shared UAVNode_ object which will appear over all the connections, or provide a _factory function_ which creates a new UAVNode for each new connection. Both are useful cases and there are big semantic differences between them; memory, security and reliability implications.

This example creates two seperate TCP servers on top of the same node, one normal (on port 66) and the other for hex debugging. (on port 67) As usual, the WiFi must be connected before trying this.
```C++
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,uav_node);
  // start a debug tcp server over the node on port 67, no oob handler
  tcp_debug = new TCPNode(67,uav_node,true,nullptr);
}
```

Using a node factory function flips the relationship - now the TCPNode server is more persistent than UAVNodes. It creates and destroys them at will, though we have to help it fully set up the node.
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

### Node Loop

The UAVNode and TCPNode objects need to be polled during the Arduino Loop function, preferably at a high rate to handle all the network traffic.

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

## Installing Standard Apps

There are reference implementations provided for several apps defined by the specification. Each app has 'installer' functions to set up a UAVNode. Some apps require extra parameters to connect them with other system or setup objects. Note that once added there are no cleanup functions to remove an app from a node. Just burn it to the ground and start again.

```C++
  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
  RegisterApp::app_v1(uav_node, system_registers);
```
Apps can add timed tasks to the node (like Heartbeat) which send regular subject broadcasts.
App classes provide API methods which are typically named for remote UAVCAN services they call. 

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

Note that service callbacks will always return eventually - thanks to timeouts - if the underlying process fails. A null reply object is provided in those error/timeout cases. The callback function will always be called once, and only once, after the result is known.

This is what makes the API functions useful - they handle all the messy buffer/stream details and give us back a fully parsed result datatype object. (or nothing at all) The request object serialization is also hidden and parameters are sanity checked. 


## Subscribing to Subjects

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

## Publishing Subject Messages

During app setup it is considered good form to let the node know that you plan to publish subjects,
even if you never actually do. There are lists that it keeps, in case anyone asks.
```C++ 
  // we will be sending messages
  node->define_subject( 
    subjectid_uavcan_node_Heartbeat_1_0,  // integer port number
    dtname_uavcan_node_Heartbeat_1_0      // PROGMEM flash string containing full canonical datatype name and version
  );
```

Sending subject messages is typically done by application 'wrapper' functions and not directly, but look like so:
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
This example has a persistent object called '_heartbeat' that it updates before serializing to a stream buffer, 
then broadcasts that buffer out via the node transports.

The node.publish() function can be given an output stream or byte block. While the pattern of fill-object/serialize-buffer is expected to be very common, some apps might gain speed from tricks like rewriting a single transmit buffer. Either is fine.

The 'on completion' function is rarely needed. In most cases you won't care if the message was queued and discarded, but if you are doing specific rate-limiting or frame timing or counting you might need to know. In this example we log it for fun.


## Services

We can attach service functions to the node. The node will start accepting requests of this datatype, 
and route the request to our function. Inline lambda functions or static object functions work equally well 
so long as they have the correct (*)(node,in,reply) signature - I think the lambda style is very readable.

Typical behavior is to decode the incoming data stream into a datatype object (which handles all the byte parsing) then do some side-effect, create a return datatype object, and serialize that into a buffer for the reply.
```C++
  uav_node->define_service( 
    serviceid_ServiceName_1_0, // integer port number
    dtname_ServiceName_1_0,    // PROGMEM flash string containing full canonical datatype name and version
    [](UAVNode& node, UAVInStream& in, UAVPortReply reply) {
      // decode the request using a datatype object
      ServiceRequest q;
      in >> q;
      // compose a reply using a datatype object
      ServiceReply r;
      // [ this is where some magic would happen and fill the reply ]
      // serialize the reply into a fixed-sized buffer. how big? you should know.
      uint8_t buffer[128];
      UAVOutStream out(buffer,128);
      out << r;
      // reply with the filled stream.
      reply(out);
    }
  );
```

Services need to run fast; calls should finish within the millisecond. No waiting around, you're on a timer at both ends. If you can't immediately reply perhaps rethink your call pattern so you can publish a subject message later when the data is available.

It is a feature of UAVCAN datatypes that you're supposed to have a very good idea of how big a buffer you'll need for each message you expect to send. In hard real-time systems there will be pages of pre-allocated buffers waiting in a queue.

If you have written a proper datatype specification, the DSDL compiler will tell you this size. In practise you can eyeball the data structure, count the bytes, go "eh, round it up by a power of two-ish" and it's your own fault if you undercounted. The output stream knows when to stop, at least. 

The code style seems a little clunky, but it's making use of compiler conventions which vastly speed up object allocation. Buffers and streams that are declared together within a function are cheap, so long as you don't need them to last beyond the current scope. If you try wrapping that minimum amount of code in another function, you're leaving scope by definition.


## Designing Micro-Services

UAVCAN provides both stateless pub/sub and request/response patterns, and using the right hammer is important to nail down a good protocol.

Some programmers love request/response (RPC) because they're deeply familiar with function calls. Some love pub/sub because they believe you can do everything with email. (Message Queues) Entire books are written on these subjects.

I think the core art is to make the delays in your application logic match up with the network delays. A primary limitation is that UAV Service requests _must_ recieve a reply within 2 seconds (default network config) and more practically need to run in  milliseconds or queues will start to back up. You can't wait around for 'back-end' requests to resolve. If you know about interrupt-level programming you should think in those terms.

Inevitably some jobs are going to take their sweet time to produce a result. Using RPC and Pub/Sub together allows a special pattern that you can't get from either alone. Consider the example of taking photographs with a camera:

If you use RPC alone you'd have to call the camera's "take a photo" API function and wait for the reply data. Taking a photo is not always fast, so using RPC we either have to wait (not good, 'cause timeout) or poll (not good, inefficient or slow) and if multiple clients all want the latest photo we must send copies in seperate network transfers, so _someone_ gets to be last and hate it.

(As an alternative to above, you negotiate with the photographer to RPC you back when the photo is ready... latency is better but that still creates multiple network copies. However now you've added 'statefulness' to your sins - the photo service has to remember stuff about the clients, forget it again tactfully, decide who comes first, and that creates a whole world of annoying logic problems with who knows what.)

If you're using stateless Pub/Sub, the camera transmits the photo data _once_ as soon as it's ready and anyone who needs the photo will be silently subscribed and listening. Low latency and efficient, that's good! But if no-one is subbed the camera doesn't actually know that, and keeps taking photos and filling the network with unwanted data. That's bad. Plus there's no way for the lurking listeners to affect the photo settings by altering ISO or shutter speed. That's not great either.

A best-of-both-worlds approach is for clients to make requests for a photo, but for the camera to reply with "yup, photo #1050 coming soon!" which it then begins to take. Later it broadcasts that photo as a pub/sub subject. If many clients request a photo they can be told to expect the same photo (The more, the merrier!) or we schedule different photos later. It's a lot like Instagram, really.

Everyone gets a ticket from the window, sometimes the tickets are the same, and when your number is called over the speaker you record the message that follows and you're done. If ticket requests stop coming, the speaker goes quiet. It's an example of a modern pattern called 'reactive queues'. 

It's also similar to the "spa bubbles switch" - if anyone hit the button recently, everyone gets bubbles.

This approach means the network camera does nothing when there's nothing to do, but everyone gets quality photos quickly when needed. Isn't that all we really wanted? 

## Datatypes and Serialization

The UAVCAN specification spends a lot of effort on datatypes. There is a Data Type Definition Language for
declaring your types in exact detail. At a practical level it still comes down to stuffing bytes into a buffer
and then getting them out again at the far end.

A key idea of UAVCAN is datatypes are not 'variable size'... not exactly. They have a fixed maximum size, but they allow _omissions_ that prune away branches of the data tree before sending. Arrays and strings can be truncated, optional blocks can be omitted. But you can't add _more_ than what was allowed, only _less_.

In a true sense the serialization scheme runs on _expectations_. There are no syntax tokens in the stream 
saying "this next value is a string" (unless you put one there) the parser just _expects_ a string to begin on cue.

If you follow the spec, it's easy to create such datatypes. It's also pretty easy to start from the implementation end and work backwards if you keep things simple. One day there may be automatic code generation (it's pretty boilerplate) but in the meantime there are five main things to declare:

- Our datatype needs a _name_, which is long and descriptive and has specific bits including version numbers. 
The name is for humans.

- This name is converted into a _datatype hash_ which is a random-ish looking number with certain neat qualities, including being fixed size and fairly short. Names always hash to the same number. The hash is for network messages.

- We need to declare an _object class_ that is the C++ representation of our datatype. It will have properties that are also datatypes.

- We need a serializer function (<< operator) which uses our data object to create a stream of bytes.

- We need a parser function (>> operator) which takes a stream of bytes and extracts our data object.

This example declares the "uavcan.node.Version.1.0" datatype which has two properties - major and minor version number parts, each one byte long. Parsing means reading the two bytes in order, serializing means writing them out in the same order. Simple as it gets.

```C++
static const     char dtname_uavcan_node_Version_1_0[] PROGMEM = "uavcan.node.Version.1.0";
static const uint64_t dthash_uavcan_node_Version_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_Version_1_0);
class NodeVersion {
    public:
        // properties
        uint8_t major;
        uint8_t minor;
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, NodeVersion& v) {
            return s >> v.major >> v.minor;
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeVersion& v) {
            return s << v.major << v.minor;
        }
};
```

A much more complex datatype is the GetInfo reply:

As well as various string and array properties you can see that it uses the NodeVersion type we just defined, multiple times. But otherwise it has the same shape... the metadata, the data object class with properties, and then the parser/serializer functions.

What's cool here is how datatypes can be used as sub-components of other datatypes. The stream code does the necessary recursion for us, and the functions stay readable. Also the code runs very fast.

```C++
static const     char    dtname_uavcan_node_GetInfo_1_0[] PROGMEM = "uavcan.node.GetInfo.1.0";
static const uint64_t    dthash_uavcan_node_GetInfo_1_0 = UAVNode::datatypehash_P(dtname_uavcan_node_GetInfo_1_0);
static const uint16_t serviceid_uavcan_node_GetInfo_1_0 = 430;
class NodeGetInfoReply {
    public:
        // properties
        NodeVersion protocol_version;
        NodeVersion hardware_version;
        NodeVersion software_version;
        uint64_t software_vcs_revision_id;
        uint8_t unique_id[16];
        std::string name;
        uint8_t software_image_crc_count;
        uint64_t software_image_crc;
        std::string certificate;
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, NodeGetInfoReply& v) { 
            s >> v.protocol_version; 
            s >> v.hardware_version; 
            s >> v.software_version; 
            s >> v.software_vcs_revision_id; 
            for(int i=0; i<16; i++) s >> v.unique_id[i]; 
            s >> v.name; 
            s >> v.software_image_crc_count;
            if(v.software_image_crc_count==1) {
                s >> v.software_image_crc;
            }
            s >> v.certificate; 
            return s; 
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const NodeGetInfoReply& v) { 
            s << v.protocol_version; 
            s << v.hardware_version; 
            s << v.software_version; 
            s << v.software_vcs_revision_id; 
            for(int i=0; i<16; i++) s << v.unique_id[i]; 
            s << v.name; 
            s << v.software_image_crc_count; 
            if(v.software_image_crc_count==1) {
                s << v.software_image_crc;
            }
            s << v.certificate; 
            return s; 
        }
};
```
There is obvious symmetry in the serializer and parser, as there should be.

The quickest way to screw up is to mis-match the logical order. Unfortunately it's your job as a programmer to get it right; it will always be faster to 'unroll' as static code than to dynamically read a definition file. 
There may one day be a DTDL-driven parser/serializer for dynamic use, but if you're writing C functions you want to 
be using C objects - your 'language bindings' are static. 

Embedded C++ has no dynamic type system (at least on GCC for Arduino SDK compilers) so no access to Reflection either. Just get it right.

And yes, the whole "friend" syntax for the stream operators is borderline C++ black magic. Just copy and paste and you'll be fine. It's pushing conventions to put all that code into a header file, but you'll want to see it together trust me.

Most services have two datatypes - one each for request and response objects. The node.GetInfo Request is empty (no parameters are needed or wanted) so only a Reply object needs to be declared in this case. In most situations you'll need both.


## Example Sketches
* HeartbeatListener.ino subscribes to heartbeat messages (including it's own) on a variety of network transports and logs them to the serial port.
* SerialOOB.ino shows how to attach "out of band" functions when setting up serial transports, in case you expect a human to also be on the line
* Order66.ino Tests Node.GetInfo and Node.ExecuteCommand and will occasionally command the execution of order 66 on nearby nodes.
