#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <libuavesp.h>

// ADC Boot Configuration
// ADC_MODE(ADC_VCC); // configure the ESP8266 to internal voltage mode, otherwise defaults to external sense

char wifi_ssid[] = "ssid";    //  your network SSID (name)
char wifi_pass[] = "pass";   // your network password


// 64-bit microsecond timer lambda function, used for hardware-level events
// deals with the 70-minute wraparound, has an offset for aligning with external clocks
unsigned long last_micros = 0;
uint64_t system_micros_offset = 0;
auto system_time_us = []()->uint64_t {
  return system_micros_offset + (uint64_t)( micros() - last_micros );
};

// millisecond timer, used for most application-level loops
unsigned long last_time = 0;


HardwareSerialPort serial_port_0(Serial);
DebugSerialPort debug_port_0(serial_port_0);
UAVNode * uav_node;
TCPNode * tcp_node;
TCPNode * tcp_debug;

// OOB handlers
void oob(char *transport_name, SerialFrame* rx, uint8_t* buf, int count) {
  Serial.print(transport_name);
  Serial.print(": ");
  Serial.write(buf, count);
  Serial.println();
}

void serial_oob(UAVTransport *transport, SerialFrame* rx, uint8_t* buf, int count) {
  oob("ser", rx, buf, count);
}

void tcp_oob(UAVTransport *transport, SerialFrame* rx, uint8_t* buf, int count) {
  oob("tcp", rx, buf, count);
}

RegisterList* system_registers = new RegisterList();

void uavcan_setup() {
  // initialize uavcan node
  uav_node = new UAVNode();
  // attach the system microsecond timer
  uav_node->get_time_us = system_time_us;

  // convert address and mask into a LSB-first uint32, which for some reason is the opposite of the WiFi object's properties
  IPAddress ip = WiFi.localIP();
  IPAddress snet = WiFi.subnetMask();
  uint32_t addr = 0;
  for(int i=0; i<4; i++) addr = (addr<<8) | ( ip[i] & ~snet[i] );
  // set the node id to the last part of the subnet ip address
  uav_node->serial_node_id = addr & UV_SERIAL_NODEID_MASK;
  
  // add a loopback serial transport for testing
  uav_node->serial_add( new SerialTransport( new LoopbackSerialPort(), true, serial_oob ) );

  // enable the serial transport on port 0 (or debug)
  //uav_node->serial_add( new SerialTransport(serial_port_0) );
  //uav_node->serial_add( new SerialTransport(debug_port_0) );

  // TCP servers are complicated because each connection could route to a unique node.
  // but for now all TCP nodes connect to the single central uav_node.
  
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,uav_node,false,tcp_oob);

  // start a debug tcp server over the node on port 67
  tcp_debug = new TCPNode(67,uav_node,true,tcp_oob);

  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
  RegisterApp::app_v1(uav_node, system_registers);

  // debug our port list
  uav_node->ports.debug_ports();

}

int jedi_purge = 1000;

void setup(){
  // initialize 64 bit microsecond timer offset
  last_micros = micros();
  system_micros_offset = last_micros;

  // initialize serial port
  Serial.begin(115200);
  // we don't want the debug output
  Serial.setDebugOutput(false);
  Serial.println();
  // start the filesystem
  // SPIFFS.begin();

  // connect to wifi
  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected ");
  Serial.print(WiFi.localIP());
  Serial.print(" / ");
  Serial.print(WiFi.subnetMask());
  Serial.println();
  // start the network node
  uavcan_setup();
  Serial.print("UAVCAN Node ");
  Serial.print(uav_node->serial_node_id);
  Serial.println();

  // attach a listener for heartbeat messages
  uav_node->subscribe(
    portid_uavcan_node_Heartbeat_1_0, 
    dthash_uavcan_node_Heartbeat_1_0, 
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
  // crc speed tests
  // test_crc32();

  // initialize millisecond timer
  last_time = millis();

  // begin main loop
}

auto info_fn = [](NodeGetInfoReply* info){
  if(info==nullptr) {
    Serial.println("node info {}");
  } else {
    Serial.println("node info {");
    Serial.print("     name : "); Serial.print(info->name.c_str()); Serial.println();
    Serial.print("      uid : ["); for(int i=0; i<16; i++) { Serial.print(info->unique_id[i],16); Serial.print(i==15?"]":" "); } Serial.println();
    Serial.print(" protocol : v"); Serial.print(info->protocol_version.major); Serial.print("."); Serial.print(info->protocol_version.minor); Serial.println();
    Serial.print(" hardware : v"); Serial.print(info->hardware_version.major); Serial.print("."); Serial.print(info->hardware_version.minor); Serial.println();
    Serial.print(" software : v"); Serial.print(info->software_version.major); Serial.print("."); Serial.print(info->software_version.minor); Serial.println();
    Serial.println("}");
  }
};

auto command_fn = [](NodeExecuteCommandReply* reply){
  if(reply==nullptr) {
    Serial.println("command result {}");
  } else {
    Serial.println("command result {");
    Serial.print(" status : "); Serial.print(reply->status); Serial.println();
    Serial.println("}");
  }
};

void loop() {
  // update 64 bit system microsecond timer offset
  unsigned long current_micros = micros();
  system_micros_offset += (current_micros-last_micros);
  last_micros = current_micros;

  // milliseconds since last loop, deals with the 50 day cyclic overflow.
  unsigned long t = millis();
  int dt = t - last_time;
  last_time = t;

  // UAVCAN node tasks
  uav_node->loop(t,dt);
  // TCP server tasks
  tcp_node->loop(t,dt);
  tcp_debug->loop(t,dt);

  //
  if(true) {
    jedi_purge-=dt;
    if(jedi_purge<0) {
      jedi_purge = 9000;
      // log stats
      Serial.print("--- heap:"); Serial.print(ESP.getFreeHeap()); Serial.print(" maxfb:"); Serial.print(ESP.getMaxFreeBlockSize()); Serial.println();
      // do our regular big task
      switch(random(4)) {
        case 0:
          NodeinfoApp::ExecuteCommand(uav_node, uav_node->serial_node_id, 66, "Execute order 66!", command_fn);
          break;
        case 1:
          NodeinfoApp::GetInfo(uav_node, 159, info_fn); // to windows machine, if available
          break;
        default:
          NodeinfoApp::GetInfo(uav_node, uav_node->serial_node_id, info_fn); // to self
          break;
      }
    }
  }
  // process OS tasks
  delay(1);
}

