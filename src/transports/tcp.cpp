#include "tcp.h"

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
    return _client->available();
}

int TCPSerialPort::writeCount() {
#ifdef ESP8266
    return _client->availableForWrite();
#endif
#ifdef ESP_PLATFORM
    return 256; // *sigh* fake it for now.
#endif
}

// serial transport over TCP

TCPSerialTransport::TCPSerialTransport (WiFiClient* client, UAVSerialPort *port, bool owner, SerialOOBHandler oob) : SerialTransport{port,owner,oob} {    
    _client = client;
    _port->println("Connected.");
    _port->flush();
}

TCPSerialTransport::~TCPSerialTransport() {
    if(unique_node!=nullptr) {
        unique_node->remove(this);
        delete unique_node;
    }
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

TCPNode::TCPNode (int server_port, UAVNode* node, bool debug, SerialOOBHandler oob) {
    _node = node; // use shared node
    _node_fn = nullptr;
    _debug = debug;
    oob_handler = oob;
    _server = new WiFiServer(server_port);
    start();
}

TCPNode::TCPNode (int server_port, std::function<UAVNode*()> node_fn, bool debug, SerialOOBHandler oob) {
    _node = nullptr;
    _node_fn = node_fn; // create unique node per connection
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
        WiFiClient * client = new WiFiClient(newClient);
        UAVSerialPort * port = new TCPSerialPort(client);
        if(_debug) {
            // wrap in a debug port, destroy them both together
            port = new DebugSerialPort(port, true);
        }
        TCPSerialTransport * serial = new TCPSerialTransport(client, port, true, oob_handler);
        _clients.push_back(serial);
        UAVNode * n = nullptr;
        if(_node) {
            n = _node; // use shared node
        } else if(_node_fn) {
            n = _node_fn(); // create unique node
            serial->unique_node = n;
        }
        if(n!=nullptr) {
            n->add(serial);
        }
    }
    // for each serial link
    auto i = _clients.begin();
    while(i!=_clients.end()) {
        TCPSerialTransport* serial = *i;
        if(serial->closed()) {
            if(_node) {
                _node->remove(serial);
            }
            i = _clients.erase(i);
            delete serial;
        } else {
            i++;
        }
    }
}
