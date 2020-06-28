#include "uv_transport.h"
#include "uv_transport_serial.h"
#include "uv_transport_canard.h"
#include "crc32c.h"
#include "numbermap.h"
#include <map>

// hardware serial port

HardwareSerialPort::HardwareSerialPort(HardwareSerial& port) {
    _port = &port;
}
HardwareSerialPort::~HardwareSerialPort() { }
void HardwareSerialPort::read(uint8_t *buffer, int count) {
    _port->readBytes(buffer,count);
}
void HardwareSerialPort::write(uint8_t *buffer, int count) {
    _port->write(buffer, count);
}
void HardwareSerialPort::flush(){
    _port->flush();
}
int HardwareSerialPort::readCount() {
    return _port->available();
}
int HardwareSerialPort::writeCount() {
    return _port->availableForWrite();
}

// loopback serial port
uint8_t _buffer[128];
int _size=128;
int _head=0;
int _tail=0;

LoopbackSerialPort::LoopbackSerialPort() { }
LoopbackSerialPort::~LoopbackSerialPort() { }
void LoopbackSerialPort::read(uint8_t *buffer, int count) {
    int c = min(count,_count);
    _count -= c;
    int i = 0;
    while(c>0) {
        buffer[i++] = _buffer[_tail++];
        if(_tail==_size) _tail=0;
        c--;
    }
}
void LoopbackSerialPort::write(uint8_t *buffer, int count) {
    int c = min(count,_size-_count);
    _count += c;
    int i = 0;
    while(c>0) {
        _buffer[_head++] = buffer[i++];
        if(_head==_size) _head=0;
        c--;
    }
}
void LoopbackSerialPort::flush() { }
int LoopbackSerialPort::readCount() { 
    return _count;
}
int LoopbackSerialPort::writeCount() { 
    return _size-_count;
}

// debug serial port

DebugSerialPort::DebugSerialPort(UAVSerialPort& port) {
    _port = &port;
    _owner = false;
}

DebugSerialPort::DebugSerialPort(UAVSerialPort* port, bool owner) {
    _port = port;
    _owner = owner;
}

DebugSerialPort::~DebugSerialPort() {
    if(_owner) delete _port;
}

void DebugSerialPort::read(uint8_t *buffer, int count) {
    _port->read(buffer,count);
}

void DebugSerialPort::write(uint8_t *buffer, int count) {
    // how many available on the port?
    int wc = _port->writeCount();
    // write what we can
    for(int i=0; i<count; i++) {
        if(wc>=3) {
            if(_mode==0) {
                write_hex(buffer[i]);
                wc-=3;
            }
            // if we filled the line
            if(_index==UV_SERIAL_DEBUG_LINE) {
                end_of_line(1);
                wc = write_line(wc);
            }
        }
    }
}

void DebugSerialPort::flush() {
    if(_index>0) {
        // we were writing a line
        end_of_line(2);
        // flush what we can now
        write_line(_port->writeCount());
    } else {
        _port->flush();
    }
}

int DebugSerialPort::readCount() {
    return _port->readCount();
}

int DebugSerialPort::writeCount() {
    // how many available on the port?
    int wc = _port->writeCount();
    // write any pending data we can
    wc = write_line(wc);
    // how many full lines can we fit?
    int lines = wc / (UV_SERIAL_DEBUG_LINE*4+4);
    // say we can take that many characters
    return lines * UV_SERIAL_DEBUG_LINE;
}

void DebugSerialPort::write_hex(uint8_t c) {
    // write hex version of char
    uint8_t hex[3];
    uint8_t a = (c>>4) & 0x0F;
    uint8_t b = c & 0x0F;
    hex[0] = (a>9) ? (a+'a'-10) : (a+'0');
    hex[1] = (b>9) ? (b+'a'-10) : (b+'0');
    hex[2] = ' ';
    _port->write(hex,3);
    // add visible char to line
    _line[_index++] = ( (c>=32) && (c<127) ) ? c : '.';
}

void DebugSerialPort::end_of_line(int mode) {
    // pad after the hex block
    int pad = (UV_SERIAL_DEBUG_LINE - _index) *3 + 2;
    _flushed = -pad;
    _mode = mode;
}

int DebugSerialPort::write_line(int wc) {
    // are we flushing the line?
    if(_mode>0) {
        // are we writing spaces?
        if(_flushed < 0) {
            // how many can we write now?
            int sc = min(wc,-_flushed);
            // write that many spaces
            uint8_t spaces[sc];
            for(int i=0; i<sc; i++) spaces[i] = ' ';
            _port->write(spaces,sc);
            // we've flushed that many
            _flushed += sc;
            wc -= sc;
        }
        if(_flushed>=0) {
            // how many can we write now?
            int fc = min(wc,_index - _flushed);
            // write that much of the line buffer
            _port->write(&_line[_flushed],fc);
            _flushed += fc;
            wc -= fc;
        }
        if(_flushed==_index) {
            if(wc>=2) {
                // write the newline
                uint8_t nl[4] = "\r\n";
                _port->write(nl,2);
                wc -= 2;
                // flush the output?
                if(_mode==2) {
                    _port->flush();
                }
                _mode = 0;
                _index = 0;
            }
        }
    }
    return wc;
}

// serial transport

SerialTransport::SerialTransport(UAVSerialPort* port, bool owner, SerialOOBHandler oob) {
    _port = port;
    _owner = owner;
    oob_handler = oob;
    _rx  = new SerialFrame();
    _rx->transfer_state = UV_SERIAL_RX_STATE_NONE;
    _rx->frame_size = UV_SERIAL_MAX_FRAME_SIZE;
    _rx->frame_index = 0;
    _rx->frame_buffer = new uint8_t[UV_SERIAL_MAX_FRAME_SIZE];
    _rx->transfer = nullptr;
    _tx = NULL;
    _queue = new NumberMap(32);
}

SerialTransport::~SerialTransport() {
    for(int i=0; i<_queue->count; i++) delete _queue->values[i];
    delete _queue;
    delete _rx->frame_buffer;
    delete _rx;
    if(_owner) delete _port;
}

void SerialTransport::loop(UAVNode& node, const unsigned long t, const int dt) {
    // is there rx data available?
    int remain = _port->readCount();
    while(remain>0) {
        // Serial.print("<"); Serial.print(remain); Serial.print(">");
        // bulk read a block
        uint8_t buf[64];
        remain = min(64,remain);
        _port->read(buf,remain);
        // parse the byte stream into a transfer buffer
        SerialTransport::parse_buffer(buf, remain, &node);
        // any left?
        remain = _port->readCount();
    }
    // is there ample space in the serial port tx buffer?
    remain = _port->writeCount();
    if(remain>=16) {
        // bulk write a block
        uint8_t buf[remain];
        int count=0;
        // are we ready to start the next transmit?
        if( (_tx==NULL) && (_queue->count>0) ){
            // start a new transfer
            _tx = (SerialFrame *)_queue->values[0];
            // send the frame start delimiter
            buf[count++] = UV_SERIAL_FRAME_DELIMITER;
            remain--;
        }
        // do we have a tx in progress
        if(_tx!=NULL) {
            uint8_t c;
            int fi = _tx->frame_index;
            int fr = _tx->frame_size - fi;
            // while there's data and we have buffer space
            while( (remain>1) && (fr>0) ) {
                // consume one byte from the transmit buffer
                c = _tx->frame_buffer[fi++];
                fr--;
                // is it an escaped character?
                switch(c) {
                    case UV_SERIAL_FRAME_DELIMITER:
                    case UV_SERIAL_ESCAPE_PREFIX:
                        // escape the byte
                        buf[count++] = UV_SERIAL_ESCAPE_PREFIX;
                        buf[count++] = c ^ 0xFF;
                        remain -= 2;
                        break;
                    default:
                        // write the byte unmodified
                        buf[count++] = c;
                        remain--;
                        break;
                }
            }
            // have we finished the transfer (and have space to write our end delimeter?)
            if( (remain>0) && (fr==0) ) {
                // send the frame end delimiter
                buf[count++] = UV_SERIAL_FRAME_DELIMITER;
                // and flush the output
                _port->write(buf,count);
                _port->flush();
                // dequeue the transfer
                _tx = NULL;
                dequeue(0);
            } else {
                // write what we got
                _port->write(buf,count);
                // store the index for next time
                _tx->frame_index = fi;
            }
        }
    }
}

void SerialTransport::parse_buffer(uint8_t* parse, int count, UAVNode* node) {
    // localize the frame state
    int state = _rx->transfer_state;
    int index = _rx->frame_index;
    int size  = _rx->frame_size;
    uint8_t* frame = _rx->frame_buffer;

    // parse buffer properties
    uint8_t* p = parse; // read pointer from parse buffer block
    int remain = count; // all of the buffer remains to be parsed
    // trasfer frame buffer properties
    uint8_t* fp = &frame[index]; // write pointer to end of frame
    int frame_remain = size-index; // how many bytes can still fit

    // used only when oob
    uint8_t* oob_start;
    int oob_size;

    // character variable, used a lot
    uint8_t ch; 
    
    while(remain>0) {
        //Serial.print(state);  Serial.print(".");
        // what state were we in?
        switch(state) {
            case UV_SERIAL_RX_STATE_NONE:
            case UV_SERIAL_RX_STATE_OOB:
                // definitely out-of-band. we can skip to the next frame delimiter
                oob_start = p; 
                oob_size = 0;
                // fragment start pointer and size counter
                while(remain>0) {
                    // consume next character
                    ch = *p++; remain--; 
                    if(ch == UV_SERIAL_FRAME_DELIMITER) {
                        // switch state to delimeter parsing
                        state = UV_SERIAL_RX_STATE_DELIMITER;
                        break;
                    } else {
                        // more data for the oob fragment
                        oob_size++;
                    }
                }
                // send known oob fragment to the handler
                if(oob_size>0) {
                    if(oob_handler!=nullptr) oob_handler(this, _rx, oob_start, oob_size);
                }
                break;
            case UV_SERIAL_RX_STATE_DELIMITER:
                // we have just seen a delimeter. we may see more.
                while(remain>0) {
                    // peek at the next character.
                    ch = *p;
                    // if it's a frame delimeter
                    if(ch == UV_SERIAL_FRAME_DELIMITER) {
                        // consume it, stay in this state.
                        p++; remain--; 
                    } else if(ch == UV_SERIAL_FRAME_VERSION_0) {
                        // we can try to parse it as a v0 frame, 
                        // timestamp the tramsfer start here
                        // auto timestamp = node->get_time_us();
                        // save that timestamp in the transfer metadata
                        // ...
                        // switch to that state and reprocess 
                        state = UV_SERIAL_RX_STATE_FRAME;
                        break;
                    } else {
                        // not a recognized protocol frame type. we are now out-of-band
                        // change to that state and reprocess the character as the first
                        state = UV_SERIAL_RX_STATE_OOB;
                        break;
                    }
                }
                break;
            case  UV_SERIAL_RX_STATE_FRAME:
                // parse data into the frame buffer as fast as possible
                while(remain>0) {
                    // consume next character
                    ch = *p++; remain--; 
                    // it the escape code?
                    if(ch == UV_SERIAL_ESCAPE_PREFIX) {
                        // is the next character available right now?
                        if(remain>0) {
                            // consume next character too
                            ch = *p++; remain--;
                            // de-escape data for the frame buffer
                            if(frame_remain>0) { *fp++ = ch^0xFF; index++; frame_remain--; } 
                        } else {
                            // no more for now, switch state for the next parse
                            state = UV_SERIAL_RX_STATE_ESCAPE;
                        }
                    // is it a frame delimeter
                    } else if(ch == UV_SERIAL_FRAME_DELIMITER) {
                        //Serial.print("} "); 
                        // end of frame. if we had any frame data update the frame
                        if(index>0) {
                            // then attempt to decode the frame content and process it through the node
                            _rx->frame_index = index;
                            decode_frame(_rx,node);
                            // clear the recieve buffer state for the next frame
                            fp = frame; index = 0; frame_remain = size;
                        }
                        //Serial.print(" { ");
                        // switch state to where missing one of the delimiters is acceptable
                        state = UV_SERIAL_RX_STATE_DELIMITER;
                        break;
                    } else {
                        // it must be more data for the frame
                        if(frame_remain>0) { *fp++ = ch; index++; frame_remain--; }
                    }
                }
                break;
            case  UV_SERIAL_RX_STATE_ESCAPE:
                // this should only happen if the last parse ended with an escape code cliffhanger
                // consume one character
                ch = *p++; remain--;
                // de-escape character code to the frame
                if(frame_remain>0) { *fp++ = ch^0xFF; index++; frame_remain--; }
                // back to regular frame parse state for the rest
                state = UV_SERIAL_RX_STATE_FRAME;
                break;
        }
    }
    // store the locals back into the frame state
    _rx->transfer_state = state;
    _rx->frame_index = index;
}

void SerialTransport::encode_frame(SerialTransfer * transfer, UAVNode *node) {
    // how big a transfer buffer will we need?
    int size = UV_SERIAL_MIN_FRAME_SIZE + transfer->payload_size;
    // allocate a buffer for it
    uint8_t* buffer = new uint8_t[size];
    // header data
    uint16_t sid = transfer->local_node_id;
    uint16_t did = transfer->remote_node_id;
    uint16_t dataspec = transfer->port_id;
    uint64_t transfer_id = transfer->transfer_id;
    uint64_t datatype = transfer->datatype;
    uint32_t frame_index = (1<<31);
    switch(transfer->transfer_kind) {
        case CanardTransferKindRequest: dataspec |= (1<<15); break;
        case CanardTransferKindResponse: dataspec |= (1<<15) | (1<<14); break;
    }
    // build the header
    buffer[0] = UV_SERIAL_FRAME_VERSION_0;
    buffer[1] = transfer->priority;
    UAVTransport::encode_uint16(&buffer[2], sid);
    UAVTransport::encode_uint16(&buffer[4], did);
    UAVTransport::encode_uint16(&buffer[6], dataspec);
    UAVTransport::encode_uint64(&buffer[8], datatype);
    UAVTransport::encode_uint64(&buffer[16], transfer_id);
    UAVTransport::encode_uint32(&buffer[24], frame_index);
    // crc the header
    UAVTransport::encode_uint32(&buffer[28], crc32c(buffer,UV_SERIAL_HEADER_WITHOUT_CRC_SIZE) );
    // copy the payload
    memcpy(&buffer[32], transfer->payload, transfer->payload_size);
    // crc the payload
    UAVTransport::encode_uint32(&buffer[32+transfer->payload_size], crc32c(&buffer[32],transfer->payload_size) );
    // store the transfer frame
    transfer->frame_data = buffer;
    transfer->frame_size = size;
}

bool SerialTransport::decode_frame(SerialFrame* rx, UAVNode *node) {
    int index = rx->frame_index;
    uint8_t* buffer = rx->frame_buffer;
    // do we have enough data for a minimum frame?
    if(index < UV_SERIAL_MIN_FRAME_SIZE) return false;
    // is this a known version?
    if(buffer[0] == UV_SERIAL_FRAME_VERSION_0) {
        // get pointers and payload size
        uint8_t * header = buffer;
        uint8_t * header_crc = &buffer[UV_SERIAL_HEADER_WITHOUT_CRC_SIZE];
        uint8_t * payload = &buffer[UV_SERIAL_HEADER_WITH_CRC_SIZE];
        uint8_t * payload_crc = &buffer[index - UV_SERIAL_CRC_SIZE];
        int payload_size = index - UV_SERIAL_HEADER_WITH_CRC_SIZE - UV_SERIAL_CRC_SIZE;
        // check header crc
        uint32_t h_crc = crc32c(header, UV_SERIAL_HEADER_WITHOUT_CRC_SIZE);
        uint32_t header_crc_value = UAVTransport::decode_uint32(header_crc);
        if(h_crc != header_crc_value) {
            // failed header crc
            return false;
        }
        // check payload crc
        uint32_t p_crc = crc32c(payload, payload_size);
        uint32_t payload_crc_value = UAVTransport::decode_uint32(payload_crc);
        if(p_crc != payload_crc_value) {
            // failed payload crc
            return false;
        }
        // decode priority
        CanardPriority priority = (CanardPriority)header[1];
        // decode node ids
        uint16_t src_node_id = UAVTransport::decode_uint16(&header[2]);
        uint16_t dst_node_id = UAVTransport::decode_uint16(&header[4]);
        // decode frame data specifier
        uint16_t dataspec = UAVTransport::decode_uint16(&header[6]);
        CanardTransferKind kind;
        uint16_t port_id;
        if( (dataspec & (1<<15)) == 0) {
            kind = CanardTransferKindMessage;
            port_id = dataspec;
        } else {
            if( (dataspec & (1<<14)) == 0) {
                kind = CanardTransferKindRequest;
            } else {
                kind = CanardTransferKindResponse;
            }
            port_id = dataspec & 0x3FFF;
        }
        // decode datatype
        uint64_t datatype  = UAVTransport::decode_uint64(&header[8]);
        // decode transfer id
        uint64_t transfer_id  = UAVTransport::decode_uint64(&header[16]);
        // decode frame index
        uint32_t frame_index  = UAVTransport::decode_uint32(&header[24]);
        // we only accept single-frame transfer at this time
        if(frame_index != (1<<31)) {
            Serial.print("badframe ");
            return false;
        }
        // the frame seems to be well formed. wrap it in a transfer header structure
        SerialTransfer transfer = {
            .timestamp_usec = 0,
            .priority = priority,
            .transfer_kind = kind,
            .port_id = port_id,
            .datatype = datatype,
            .local_node_id = dst_node_id,
            .remote_node_id = src_node_id,
            .transfer_id = transfer_id,
            .payload_size = payload_size,
            .payload = payload
        };
        // pass it to the common node serial transfer reciever, this may cause a lot of activity.
        node->serial_receive(&transfer);
        // the transfer is considered complete at this time, the transfer wrapper is destroyed and the buffer is recycled
        return true;
    }
    // we cannot decode future protocol versions
    return false;
}

void SerialTransport::send(SerialTransfer* transfer) {
    // the transfer is ready to go. wrap it in a SerialFrame
    SerialFrame *frame = new SerialFrame();
    frame->transfer = transfer;
    frame->transfer_state = UV_SERIAL_RX_STATE_NONE;
    frame->frame_size = transfer->frame_size;
    frame->frame_index = 0;
    frame->frame_buffer = transfer->frame_data;
    // insert it into the transfer queue
    transfer->frame_usage++;
    _queue->insert(transfer->priority, frame);
    // if the queue is now full...
    if(_queue->count==_queue->size) {
        // remove the message with the least priority
        dequeue(_queue->count-1);
    }
}

void SerialTransport::dequeue(int index) {
    // find that entry
    SerialFrame *frame = (SerialFrame *)_queue->values[index];
    if(frame==NULL) return;
    // remove the entry from the queue
    _queue->remove_index(index);
    // destroy the frame state container, but keep the transfer
    SerialTransfer* transfer = frame->transfer;
    delete frame;
    // decrement the transfer usage
    transfer->frame_usage--;
    // if the counter has hit zero, the transfer is complete
    if(transfer->frame_usage==0) transfer_complete(transfer);
}

void SerialTransport::transfer_complete(SerialTransfer* transfer) {
    // notify the transfer callback
    if(transfer->on_complete) transfer->on_complete();
    // destroy the transfer storage
    delete transfer->frame_data;
    delete transfer;
}