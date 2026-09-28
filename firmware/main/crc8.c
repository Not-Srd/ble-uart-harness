#include "crc8.h"

uint8_t crc8(const uint8_t data[], size_t len){
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++){
    crc ^= data[i];
    for (int k = 0; k < 8; k++) {
      if (crc & 0x80) {
        crc = crc << 1;
        crc ^= 0x07;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc;
}

