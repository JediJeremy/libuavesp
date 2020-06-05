
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

