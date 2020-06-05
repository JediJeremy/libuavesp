#ifndef UV_CRC32_H_INCLUDED
#define UV_CRC32_H_INCLUDED

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif


//uint32_t crc32c_flash(uint8_t *buf, int len);
//uint32_t crc32c_ram(uint8_t *buf, int len);
uint32_t crc32c(uint8_t *buf, int len);

//void test_crc32();

#ifdef __cplusplus
}
#endif

#endif