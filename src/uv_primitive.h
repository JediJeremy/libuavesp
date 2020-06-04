#ifndef UV_APP_PRIMITIVE_H_INCLUDED
#define UV_APP_PRIMITIVE_H_INCLUDED

#include "uv_common.h"
#include "uv_node.h"
#include <string>

// primitive types
static const char     dtname_uavcan_primitive_Empty_1_0[] PROGMEM = "uavcan.primitive.Empty.1.0";
static const char     dtname_uavcan_primitive_String_1_0[] PROGMEM = "uavcan.primitive.String.1.0";
static const char     dtname_uavcan_primitive_Unstructured_1_0[] PROGMEM = "uavcan.primitive.Unstructured.1.0";

// utility methods for dealing with 16-bit floating point numbers
float    fp16_to_float(uint16_t f);
uint16_t float_to_fp16(float f);
// utility method for fastest-possible determination of MSB set of byte value. returns bit index from 1..8, or 0 if all zero bits.
uint8_t  first_uint8_onebit(uint8_t v);


class UAVInStream {
    public:
        uint8_t* input_buffer;
        int input_size;
        int input_index;
        int input_remain;
        UAVInStream(uint8_t* buffer, int size) {
            input_buffer = buffer;
            input_size = size;
            input_index = 0;
            input_remain = size;
        }
        void input_memcpy(void* payload, int length);
        friend UAVInStream& operator>>(UAVInStream& s, int8_t& v) { s.input_memcpy((void *)&v,1); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int16_t& v) { s.input_memcpy((void *)&v,2); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int32_t& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, int64_t& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint8_t& v) { s.input_memcpy((void *)&v,1); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint16_t& v) { s.input_memcpy((void *)&v,2); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint32_t& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, uint64_t& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, float& v) { s.input_memcpy((void *)&v,4); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, double& v) { s.input_memcpy((void *)&v,8); return s; }
        friend UAVInStream& operator>>(UAVInStream& s, std::string& v) { 
            uint8_t c;
            s.input_memcpy((void *)&c,1);
            v.resize(c);
            s.input_memcpy((void *)v.data(),(int)c); return s; 
        }
};

class UAVOutStream {
    public:
        uint8_t* output_buffer;
        int output_size;
        int output_index;
        int output_remain;
        UAVOutStream(uint8_t* buffer, int size) {
            output_buffer = buffer;
            output_size = size;
            output_index = 0;
            output_remain = size;
        }
        void output_memcpy(const void* payload, int length);
        void output_memcpy_P(PGM_P payload, int length);
        void P(PGM_P text);
        void P1(PGM_P text, int limit);
        void P2(PGM_P text, int limit);
        friend UAVOutStream& operator<<(UAVOutStream& s, const int8_t& v) { s.output_memcpy((void *)&v,1); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int16_t& v) { s.output_memcpy((void *)&v,2); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int32_t& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const int64_t& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint8_t& v) { s.output_memcpy((void *)&v,1); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint16_t& v) { s.output_memcpy((void *)&v,2); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint32_t& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const uint64_t& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const float& v) { s.output_memcpy((void *)&v,4); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const double& v) { s.output_memcpy((void *)&v,8); return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const char v[]) {
            uint8_t c = min(0xFF, (int)strlen(v));
            s.output_memcpy((void *)&c,1);
            s.output_memcpy((void *)v,(int)c);
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const std::string& v) { /// remove 'const' for greatest number of errors!
            auto len = v.size();
            uint8_t c = min(0xFF, (int)len);
            s.output_memcpy((void *)&c,1);
            s.output_memcpy((void *)v.data(),(int)c);
            return s; 
        }
        // friend UAVOutStream& operator<<(UAVOutStream& s, const PGM_P& v) { s.output_memcpy_P(v,strlen_P(v)); return s; }
};

/*
class UAVPrimitiveEmpty {
    public:
        friend UAVInStream& operator>>(UAVInStream& s, UAVPrimitiveEmpty& v) { return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const UAVPrimitiveEmpty& v) { return s; }
};
*/

template <typename T, int MAX>
class UAVPrimitiveString {
    public:
        int size;
        uint8_t *data;
        void link(char s[]) { size = strlen(s); data = (uint8_t *)s; }
        void link(std::string s) { size = s.length; data = (uint8_t *)(s.data); }
        friend UAVInStream& operator>>(UAVInStream& s, UAVPrimitiveString<T,MAX>& v) { 
            // read array size first through the templated type
            T c;
            s >> c;
            // are there that many valid bytes left?
            if(s.input_remain>=c) {
                // store reference to the buffer string data
                v.size = c;
                v.data = &s.input_buffer[s.input_index];
                // consume that many stream bytes
                s.input_index += c;
            } else {
                // insufficient space! leave no partial copies.
                s.input_remain = 0;
            }
            // chain
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const UAVPrimitiveString<T,MAX>& v) { 
            // write the array size first through the templated type
            T c = min(v.size,MAX);
            s << c;
            // write the string contents
            s.output_memcpy( v.data, (size_t)c );
            // chain
            return s; 
        }
};

template <typename L, typename T>
class UAVPrimitiveArray {
    public:
        uint8_t* array_data;
        int array_size;
        int array_limit;
        // constructors
        UAVPrimitiveArray() { 
            array_data = nullptr;
            array_size = 0;
            array_limit = 0;
        }
        UAVPrimitiveArray(T buffer[], int limit) { 
            array_data = (uint8_t*)buffer;
            array_size = limit;
            array_limit = limit;
        }
        UAVPrimitiveArray(T buffer[], int size, int limit) { 
            array_data = (uint8_t*)buffer;
            array_size = size;
            array_limit = limit;
        }
        // element pointers
        // T* element(int index) { return &array_data[index * sizeof(T)]; }
        // uint8_t* element(int index) { return &array_data[index * sizeof(T)]; }
        // parser
        friend UAVInStream& operator>>(UAVInStream& s, UAVPrimitiveArray<L,T>& v) { 
            // read array size first
            L c;
            s >> c;
            // if there are more entries than we expected. we likely have a data type error, or a stream corruption
            int chunk = min((int)c, v.array_limit);
            // are there that many valid bytes left?
            int chunk_bytes = chunk * sizeof(T);
            if(s.input_remain>=chunk_bytes) {
                // bulk copy what will fit
                v.array_size = chunk;
                s.input_memcpy(v.array_data, chunk_bytes);
                // consume unused stream bytes
                int stream_bytes = (c-chunk) * sizeof(T);
                s.input_index += stream_bytes;
                s.input_remain -= stream_bytes;
            } else {
                // insufficient data for the array! Unexpected end of data stream.
                // That's a protocol error but for now clean out the input buffer
                s.input_remain = 0;
                v.array_size = 0;
            }
            // chain
            return s; 
        }
        // serializer
        friend UAVOutStream& operator<<(UAVOutStream& s, const UAVPrimitiveArray<L,T>& v) { 
            // write the array size first
            L c = v.array_size;
            s << c;
            // bulk write stream data
            s.output_memcpy(v.array_data,v.array_size*sizeof(T));
            // chain
            return s; 
        }
};


// primitive type array types
class UAVArrayInteger8 : public UAVPrimitiveArray<uint16_t, int8_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayInteger16 : public UAVPrimitiveArray<uint8_t, int16_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayInteger32 : public UAVPrimitiveArray<uint8_t, int32_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayInteger64 : public UAVPrimitiveArray<uint8_t, int64_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayNatural8 : public UAVPrimitiveArray<uint16_t, uint8_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayNatural16 : public UAVPrimitiveArray<uint8_t, uint16_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayNatural32 : public UAVPrimitiveArray<uint8_t, uint32_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayNatural64 : public UAVPrimitiveArray<uint8_t, uint64_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayReal16 : public UAVPrimitiveArray<uint8_t, uint16_t> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayReal32 : public UAVPrimitiveArray<uint8_t, float> { using UAVPrimitiveArray::UAVPrimitiveArray; };
class UAVArrayReal64 : public UAVPrimitiveArray<uint8_t, double> { using UAVPrimitiveArray::UAVPrimitiveArray; };


#endif