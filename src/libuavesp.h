#ifndef LIBUAVESP_H_INCLUDED
#define LIBUAVESP_H_INCLUDED

#include <Arduino.h>
#ifdef ESP8266 // ARDUINO_ARCH_ESP8266
  #include <ESP8266WiFi.h>
#endif
#ifdef ESP_PLATFORM // ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#endif

#include "node.h"
#include "transport.h"
#include "transports/serial.h"
#include "transports/udp.h"
#include "transports/tcp.h"
#include "primitive.h"
#include "apps/heartbeat.h"
#include "apps/nodeinfo.h"
#include "apps/portinfo.h"
#include "apps/register.h"

#endif