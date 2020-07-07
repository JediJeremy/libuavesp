#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <libuavesp.h>

char wifi_ssid[] = "ssid";   // your network SSID (name)
char wifi_pass[] = "pass";   // your network password


UAVNode * uav_node;
TCPNode * tcp_node;
TCPNode * tcp_debug;

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
  uav_node->add( new PortUDPTransport() );
  // start a tcp server over the node on port 66
  tcp_node = new TCPNode(66,uav_node);
  // start a hex debug tcp server over the node on port 67
  tcp_debug = new TCPNode(67,uav_node,true,nullptr);

  // start UAVCAN apps on the node
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
    delay(500); Serial.print(".");
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

  // attach a listener for heartbeat messages
  uav_node->subscribe(
    subjectid_uavcan_node_Heartbeat_1_0, 
    dtname_uavcan_node_Heartbeat_1_0, // dthash_uavcan_node_Heartbeat_1_0, 
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

  // initialize millisecond timer
  last_time = millis();
  // begin main loop
}

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

  // process OS tasks
  delay(1);
}
