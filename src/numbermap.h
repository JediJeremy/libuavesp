#ifndef NUMBERMAP_H_INCLUDED
#define NUMBERMAP_H_INCLUDED

#include "common.h"

typedef void (*NumberMapCall)(void* param);
typedef struct {
    int index;
    int count;
} NumberMapRange;

// integer map object
class NumberMap
{
    public:
        // variables
        int* keys;
        void** values;
        int count = 0;
        int size = 0;
        // constructors
        NumberMap(int size);
        ~NumberMap();
        // methods
        bool insert(int key, void* value);
        void remove_entries(int index, int count);
        void remove_index(int index);
        int find_one(int key);
        int find_first(int index, int key);
        int find_last(int index, int key);
        bool remove(int key, void* value);
        int remove_all(int key);
        bool for_each(int key, NumberMapCall fn);
        bool call_each(int key, void* param);
        void debug();
};

#endif