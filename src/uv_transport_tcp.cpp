#include "uv_transport_tcp.h"

// TCPTransport *uv_tcp;


TCPSerialPort::TCPSerialPort(WiFiClient* client) {
    _client = client;
}
TCPSerialPort::~TCPSerialPort() { }

void TCPSerialPort::read(uint8_t *buffer, int count) {
    _client->readBytes(buffer,count);
}

void TCPSerialPort::write(uint8_t *buffer, int count) {
    _client->write(buffer,count);
}

void TCPSerialPort::flush() {
    _client->flush();
}

int TCPSerialPort::readCount() {
    // if(_client->connected()==0) return 0;
    return _client->available();
}

int TCPSerialPort::writeCount() {
    // if(_client->connected()==0) return 0;
    return _client->availableForWrite();
}

// serial transport over TCP

TCPSerialTransport::TCPSerialTransport (WiFiClient* client, UAVSerialPort *port, bool owner, SerialOOBHandler oob) : SerialTransport{port,owner,oob} {    
    _client = client;
    _port->println("Connected.");
    _port->flush();
}

TCPSerialTransport::~TCPSerialTransport() {
    //Serial.println("destroy client");
    close();
    delete _client;
}

void TCPSerialTransport::close() {
    if(_client->connected()!=0) _client->stop();
}

bool TCPSerialTransport::closed() {
    return _client->connected() == 0;
}


// TCP server which creates serial transports per client

TCPNode::TCPNode (int server_port, UAVNode * node, bool debug, SerialOOBHandler oob) {
    _node = node;
    _debug = debug;
    oob_handler = oob;
    _server = new WiFiServer(server_port);
    start();
}

TCPNode::~TCPNode() {
    stop();
    delete _server;
}

bool TCPNode::start() {
    _server->begin();
    //Serial.println("new server");
    return true;
}

bool TCPNode::stop() {
    for(TCPSerialTransport *c : _clients) {
        delete c;
    }
    _server->stop();
    return true;
}

void TCPNode::loop(const unsigned long t, const int dt) {
    // Check if a new client has connected
    WiFiClient newClient = _server->available();
    if(newClient) {
        //Serial.println("new client");
        WiFiClient * client = new WiFiClient(newClient);
        UAVSerialPort * port = new TCPSerialPort(client);
        if(_debug) {
            // wrap in a debug port, destroy them both together
            port = new DebugSerialPort(port, true);
        }
        TCPSerialTransport * serial = new TCPSerialTransport(client, port, true, oob_handler);
        _clients.push_back(serial);
        if(_node) {
            _node->serial_add(serial);
        }
        //Serial.println("client created");
    }
    // for each serial link
    auto i = _clients.begin();
    while(i!=_clients.end()) {
        TCPSerialTransport* serial = *i;
        if(serial->closed()) {
            //Serial.println("client closed");
            if(_node) {
                _node->serial_remove(serial);
            }
            i = _clients.erase(i);
            delete serial;
        } else {
            // Serial.println("client loop");
            // serial->loop(t,dt);
            i++;
        }
    }
}
