#include <Arduino.h>
#include <math.h>
#include <libuavesp.h>

char wifi_ssid[] = "ssid";   // your network SSID (name)
char wifi_pass[] = "pass";   // your network password

UAVNode * uav_node;
TCPNode * tcp_node;
TCPNode * tcp_debug;

RegisterList* system_registers = new RegisterList();

// millisecond timer, used for most application-level loops
unsigned long last_time = 0;
// 64-bit microsecond timer lambda function, used for hardware-level events
// deals with the 70-minute wraparound, has an offset for aligning with external clocks
unsigned long last_micros = 0;
uint64_t system_micros_offset = 0;
auto system_time_us = []()->uint64_t {
  return system_micros_offset + (uint64_t)( micros() - last_micros );
};

void uavcan_setup() {
  // initialize uavcan node
  uav_node = new UAVNode();
  // attach the system microsecond timer
  uav_node->get_time_us = system_time_us;
  // set the node ID from the WiFi connection
  uav_node->set_id(WiFi);

  // add a loopback serial transport for testing
  uav_node->add( new SerialTransport( new LoopbackSerialPort() ) );
  // start the UDP transport for the node, since we have network
  uav_node->add( new PortUDPTransport(66) );  
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,uav_node);
  // start a debug tcp server over the node on port 67
  tcp_debug = new TCPNode(67,uav_node,true,nullptr);

  // start UAVCAN apps on the node, some of which will begin emitting messages
  HeartbeatApp::app_v1(uav_node);
  NodeinfoApp::app_v1(uav_node);
}

void setup(){
  // initialize 64 bit microsecond timer offset
  last_micros = micros();
  system_micros_offset = last_micros;

  // initialize serial port
  Serial.begin(115200);
  // we don't want the debug output
  Serial.setDebugOutput(false);
  Serial.println();

  // connect to wifi
  WiFi.mode(WIFI_STA);
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
  Serial.print(uav_node->local_node_id);
  Serial.println();

  // initialize millisecond timer
  last_time = millis();
  // begin main loop
}

// auto info_fn = [](NodeGetInfoReply* info){
void info_fn(NodeGetInfoReply* info) {
  if(info==nullptr) {
    Serial.println("node info { null }");
  } else {
    Serial.print("node info {");
    Serial.print(" name:"); Serial.print(info->name.c_str());
    Serial.print(" uid:["); for(int i=0; i<16; i++) { Serial.print(info->unique_id[i],16); Serial.print(i==15?"]":" "); }
    Serial.print(" protocol:v"); Serial.print(info->protocol_version.major); Serial.print("."); Serial.print(info->protocol_version.minor);
    Serial.print(" hardware:v"); Serial.print(info->hardware_version.major); Serial.print("."); Serial.print(info->hardware_version.minor);
    Serial.print(" software:v"); Serial.print(info->software_version.major); Serial.print("."); Serial.print(info->software_version.minor);
    Serial.println(" }");
  }
};

// auto command_fn = [](NodeExecuteCommandReply* reply){
void command_fn(NodeExecuteCommandReply* reply) {
  if(reply==nullptr) {
    Serial.println("command result { null }");
  } else {
    Serial.print("command result {");
    Serial.print(" status:"); Serial.print(reply->status);
    Serial.println(" }");
  }
};

// task timers
int debug_heap = 1000;
int jedi_purge = 2000;

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

  // debug heap 'task'
  if(true) {
    debug_heap -= dt;
    if(debug_heap<0) {
      debug_heap = 30000;
      // debug heap stats
      Serial.print("--- heap:"); Serial.print(ESP.getFreeHeap()); 
      // Serial.print(" maxfb:"); Serial.print(ESP.getMaxFreeBlockSize()); 
      Serial.println();
    }
  }
  // jedi purge 'task'
  if(true) {
    jedi_purge-=dt;
    if(jedi_purge<0) {
      jedi_purge = 2000;
      // do our regular big task
      switch(random(5)) {
        case 0:  NodeinfoApp::GetInfo(uav_node, 118, info_fn); break;
        case 1:  NodeinfoApp::GetInfo(uav_node, 232, info_fn); break;
        case 2:  NodeinfoApp::ExecuteCommand(uav_node, 118, 66, "Execute order 66!", command_fn); break;
        case 3:  NodeinfoApp::ExecuteCommand(uav_node, 232, 66, "Execute order 66!", command_fn); break;
        default: NodeinfoApp::ExecuteCommand(uav_node, 159, 66, "Execute order 66!", command_fn); break;
        // NodeinfoApp::GetInfo(uav_node, uav_node->local_node_id, info_fn); // to local, should definitely be available
      }
    }
  }
  // process OS tasks
  delay(1);
}
