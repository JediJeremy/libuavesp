#ifndef LIBUAVESP_PRIMITIVE_H_INCLUDED
#define LIBUAVESP_PRIMITIVE_H_INCLUDED

#include "common.h"
#include "node.h"
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