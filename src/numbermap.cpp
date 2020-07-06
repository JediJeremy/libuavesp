#include "numbermap.h"

NumberMap::NumberMap(int length) {
    size = length;
    //_keys = (int*) malloc(sizeof(int)*size);
    //_values = malloc(sizeof(void*)*size);
    keys = new int[length];
    values = new void*[length];
}

NumberMap::~NumberMap() {
    delete[] keys;
    delete[] values;
}

bool NumberMap::insert(int key, void* value) {
    if(count<size) {
        int i = count;
        count++;
        int j = count;
        bool more = (i>0);
        // back down through the list until we find our slot
        while(more) {
            i--;
            j--;
            if(keys[i]>key) {
                // move entry up
                keys[j] = keys[i];
                values[j] = values[i];
                if(i==0) {
                    more = false; // insert first
                }
            } else {
                i=j; 
                more=false; // insert after
            }
        }
        // insert entry
        keys[i] = key;
        values[i] = value;
        return true;
    }
    // not enough space
    return false;
}

void NumberMap::remove_entries(int index, int length) {
    // limit to map range
    if(index<0) {
        length += index;
        index = 0;
    };
    length = min(length, count-index);
    // anything to do?
    if(length<=0) return;
    // sanity check
    int remain = count - (index+length);
    if(remain<0) return;
    // do a fast memmove of both blocks
    if(remain>0) {
        memmove(&keys[index], &keys[index+length], sizeof(int)*remain );
        memmove(&values[index], &values[index+length], sizeof(void *)*remain );
    }
    // removed
    count -= length;
}

void NumberMap::remove_index(int index) {
    // remove single entry
    remove_entries(index, 1);
}

int NumberMap::find_one(int key) {
    // use a recursive approximation search to find one of the key entries
    if(count>0) {
        int a = 0; 
        int b = count-1;
        int c,k;
        while(a<b) {
            c = (a+b)>>1;
            k = keys[c];
            // is the center value our key?
            if(k==key) return c;
            // which half?
            if(k<key) {
                a = c; // recurse for top half
            } else {
                b = c; // recurse for lower half
            }
        }
        // range is now one value. did we find it?
        if(keys[a]==key) {
            return a;
        }
    }
    // didn't find it
    return -1;
}

int NumberMap::find_first(int index, int key) {
    int i = index;
    int j = i-1;
    while(i>0) {
        if(keys[j]==key) {
            i--; j--;
        } else {
            break;
        }
    }
    return i;
}

int NumberMap::find_last(int index, int key) {
    int i = index;
    int j = i+1;
    while(i<count) {
        if(keys[j]==key) {
            i++; j++;
        } else {
            break;
        }
    }
    return i;
}

bool NumberMap::remove(int key, void* value) {
    int i = find_one(key);
    if(i==-1) return false;
    int a = find_first(i,key);
    int b = find_last(i,key);
    while(a<=b) {
        if(values[a]==value) {
            remove_entries(a, 1);
            return true;
        } else {
            a++;
        }
    }
    // didn't find it
    return false;
}

int NumberMap::remove_all(int key) {
    int i = find_one(key);
    if(i==-1) return 0;
    int a = find_first(i,key);
    int b = find_last(i,key);
    int count = b-a+1;
    remove_entries(a, count);
    return count;
}

bool NumberMap::for_each(int key, NumberMapCall fn) {
    int i = find_one(key);
    if(i==-1) return false;
    int a = find_first(i,key);
    int b = find_last(i,key);
    while(a<=b) {
        if(fn!=NULL) fn(values[a]);
        a++;
    }
    return true;
}

bool NumberMap::call_each(int key, void* param) {
    int i = find_one(key);
    if(i==-1) return false;
    int a = find_first(i,key);
    int b = find_last(i,key);
    NumberMapCall fn;
    while(a<=b) {
        fn = (NumberMapCall)values[a];
        if(fn!=NULL) fn(param);
        a++;
    }
    return true;
}

/*
void NumberMap::debug() {
    Serial.print("Map count: ");
    Serial.println(count);
    for(int i=0; i<count; i++) {
        Serial.print("  ");
        Serial.print(keys[i]);
        Serial.print(" : ");
        Serial.println((uint32_t)values[i]);
    }
}
*/