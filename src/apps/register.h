#ifndef LIBUAVESP_APP_REGISTER_H_INCLUDED
#define LIBUAVESP_APP_REGISTER_H_INCLUDED

#include "../common.h"
#include "../node.h"
#include "../primitive.h"

static const char     dtname_uavcan_register_Name_1_0[] PROGMEM = "uavcan.register.Name.1.0";
class RegisterName : public UAVPrimitiveString<uint8_t,50> { };

static const char     dtname_uavcan_register_Value_1_0[] PROGMEM = "uavcan.register.Value.1.0";
class RegisterValue {
    public:
        uint8_t tag;
        union {
            void *ptr;
            // UAVPrimitiveEmpty* empty;
            UAVPrimitiveString<uint16_t,256>* str;
            UAVArrayInteger8 * aint8;
            UAVArrayInteger16 * aint16;
            UAVArrayInteger32 * aint32;
            UAVArrayInteger64 * aint64;
            UAVArrayNatural8 * anat8;
            UAVArrayNatural16 * anat16;
            UAVArrayNatural32 * anat32;
            UAVArrayNatural64 * anat64;
            UAVArrayReal16 * afp16;
            UAVArrayReal32 * afp32;
            UAVArrayReal64 * afp64;
        };
        bool owner = false;
        RegisterValue(uint8_t type_tag, void * pointer) {
            tag = type_tag;
            ptr = pointer;
        }
        ~RegisterValue() {
            reset();
        }
        void reset() {
            if(owner) {
                switch(tag) {
                    case  0:  break; // uavcan.primitive.Empty.1.0 empty   
                    case  1: delete str; break; // uavcan.primitive.String.1.0 
                    case  2: // uavcan.primitive.Unstructured.1.0 
                    case  3: // uavcan.primitive.array.Bit.1.0 
                    case  4: delete aint64; break; // uavcan.primitive.array.Integer64.1.0
                    case  5: delete aint32; break; // uavcan.primitive.array.Integer32.1.0
                    case  6: delete aint16; break; // uavcan.primitive.array.Integer16.1.0
                    case  7: delete aint8;  break; // uavcan.primitive.array.Integer8.1.0
                    case  8: delete anat64; break; // uavcan.primitive.array.Natural64.1.0
                    case  9: delete anat32; break; // uavcan.primitive.array.Natural32.1.0
                    case 10: delete anat16; break; // uavcan.primitive.array.Natural16.1.0
                    case 11: delete anat8;  break; // uavcan.primitive.array.Natural8.1.0
                    case 12: delete afp64;  break; // uavcan.primitive.array.Real64.1.0
                    case 13: delete afp32;  break; // uavcan.primitive.array.Real32.1.0
                    case 14: delete afp16;  break; // uavcan.primitive.array.Real16.1.0
                }
                tag = 0;
                owner = false;
            }
        }

        friend UAVInStream& operator>>(UAVInStream& s, RegisterValue& v) { 
            v.reset();
            s >> v.tag;
            switch(v.tag) {
                case  0: break; // uavcan.primitive.Empty.1.0 empty
                case  1: v.str = new UAVPrimitiveString<uint16_t,256>(); s >> *(v.str); break; // uavcan.primitive.String.1.0 
                case  2: break; // uavcan.primitive.Unstructured.1.0 
                case  3: break; // uavcan.primitive.array.Bit.1.0 
                case  4: v.aint64 = new UAVArrayInteger64(); s >> *(v.aint64); break; // uavcan.primitive.array.Integer64.1.0
                case  5: v.aint32 = new UAVArrayInteger32(); s >> *(v.aint32); break; // uavcan.primitive.array.Integer32.1.0
                case  6: v.aint16 = new UAVArrayInteger16(); s >> *(v.aint16); break; // uavcan.primitive.array.Integer16.1.0
                case  7: v.aint8 = new UAVArrayInteger8();   s >> *(v.aint8);  break; // uavcan.primitive.array.Integer8.1.0
                case  8: v.anat64 = new UAVArrayNatural64(); s >> *(v.anat64); break; // uavcan.primitive.array.Natural64.1.0
                case  9: v.anat32 = new UAVArrayNatural32(); s >> *(v.anat32); break; // uavcan.primitive.array.Natural32.1.0
                case 10: v.anat16 = new UAVArrayNatural16(); s >> *(v.anat16); break; // uavcan.primitive.array.Natural16.1.0
                case 11: v.anat8 = new UAVArrayNatural8();   s >> *(v.anat8);  break; // uavcan.primitive.array.Natural8.1.0
                case 12: v.afp64 = new UAVArrayReal64();     s >> *(v.afp64);  break; // uavcan.primitive.array.Real64.1.0
                case 13: v.afp32 = new UAVArrayReal32();     s >> *(v.afp32);  break; // uavcan.primitive.array.Real32.1.0
                case 14: v.afp16 = new UAVArrayReal16();     s >> *(v.afp16);  break; // uavcan.primitive.array.Real16.1.0
            }
            v.owner = true;
            return s; 
        }

        friend UAVOutStream& operator<<(UAVOutStream& s, const RegisterValue& v) { 
            s << v.tag;
            switch(v.tag) {
                case  0: break; // uavcan.primitive.Empty.1.0 empty   
                case  1: s << *(v.str); break; // uavcan.primitive.String.1.0 
                case  2: // uavcan.primitive.Unstructured.1.0 
                case  3: // uavcan.primitive.array.Bit.1.0 
                case  4: s << *(v.aint64); break; // uavcan.primitive.array.Integer64.1.0
                case  5: s << *(v.aint32); break; // uavcan.primitive.array.Integer32.1.0
                case  6: s << *(v.aint16); break; // uavcan.primitive.array.Integer16.1.0
                case  7: s << *(v.aint8);  break; // uavcan.primitive.array.Integer8.1.0
                case  8: s << *(v.anat64); break; // uavcan.primitive.array.Natural64.1.0
                case  9: s << *(v.anat32); break; // uavcan.primitive.array.Natural32.1.0
                case 10: s << *(v.anat16); break; // uavcan.primitive.array.Natural16.1.0
                case 11: s << *(v.anat8);  break; // uavcan.primitive.array.Natural8.1.0
                case 12: s << *(v.afp64);  break; // uavcan.primitive.array.Real64.1.0
                case 13: s << *(v.afp32);  break; // uavcan.primitive.array.Real32.1.0
                case 14: s << *(v.afp16);  break; // uavcan.primitive.array.Real16.1.0
            }
            return s; 
        }
};


static const char     dtname_uavcan_register_Access_1_0[] PROGMEM = "uavcan.register.Access.1.0";
static const uint64_t dthash_uavcan_register_Access_1_0 = UAVNode::datatypehash_P(dtname_uavcan_register_Access_1_0);
static const uint16_t serviceid_uavcan_register_Access_1_0 = 384;
class RegisterAccessRequest {
    public:
        RegisterName name;
        RegisterValue value;

        friend UAVInStream& operator>>(UAVInStream& s, RegisterAccessRequest& v) { 
            s >> v.name;
            s >> v.value;
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const RegisterAccessRequest& v) { 
            s << v.name;
            s << v.value;
            return s; 
        }
};

class RegisterAccessReply {
    public:
        uint64_t timestamp;
        bool is_mutable;
        bool is_persistent;
        RegisterValue value;

        friend UAVInStream& operator>>(UAVInStream& s, RegisterAccessReply& v) { 
            uint8_t flags_1;
            s >> v.timestamp;
            s >> flags_1;
            s >> v.value;
            v.is_mutable = (0x80 & flags_1)!=0;
            v.is_persistent = (0x40 & flags_1)!=0;
            return s; 
        }
        friend UAVOutStream& operator<<(UAVOutStream& s, const RegisterAccessReply& v) { 
            uint8_t flags_1 = (v.is_mutable ? 0x80 : 0) | (v.is_persistent ? 0x40 : 0);
            s << v.timestamp;
            s << flags_1;
            s << v.value;
            return s; 
        }
};

static const char     dtname_uavcan_register_List_1_0[] PROGMEM = "uavcan.register.List.1.0";
static const uint64_t dthash_uavcan_register_List_1_0 = UAVNode::datatypehash_P(dtname_uavcan_register_List_1_0);
static const uint16_t serviceid_uavcan_register_List_1_0 = 385;
class RegisterListRequest {
    public:
        uint16_t index;
        friend UAVInStream& operator>>(UAVInStream& s, RegisterListRequest& v) { s >> v.index; return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const RegisterListRequest& v) { s << v.index; return s; }
};

class RegisterListReply {
    public:
        RegisterName name;
        friend UAVInStream& operator>>(UAVInStream& s, RegisterListReply& v) { s >> v.name; return s; }
        friend UAVOutStream& operator<<(UAVOutStream& s, const RegisterListReply& v) { s << v.name; return s; }
};

class RegisterDefinition {
    public:
        PGM_P register_name;
        int   register_name_length;
        int   register_tag;
        int   data_length;
        std::function<uint8_t*()> get_data;
        std::function<void(uint8_t*)> set_data;
};
//UAVInStream& operator>>(UAVInStream& s, RegisterDefinition& v) { s >> v.name; return s; }
//UAVOutStream& operator<<(UAVOutStream& s, const RegisterDefinition& v) { s << v.name; return s; }

class RegisterList : public std::map<uint16_t, RegisterDefinition*> {
    public:
        RegisterDefinition* claim(PGM_P name) {}
};


class RegisterApp {
    public:
        static void app_v1(UAVNode *node, RegisterList *registers);
};

#endif