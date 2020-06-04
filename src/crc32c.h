#ifndef UV_CRC32_H_INCLUDED
#define UV_CRC32_H_INCLUDED

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t crc32c(uint8_t *buf, int len);

#ifdef __cplusplus
}
#endif

#endif