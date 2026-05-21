//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#include <Arduino.h>

#include "crc.h"

void ModbusCRC::init()
{
    this->crc = 0xFFFF;
}

void  ModbusCRC::update(const uint8_t *buf, size_t len) {
  for (size_t pos = 0; pos < len; pos++) {
    this->crc ^= (uint16_t)buf[pos];
    for (int i = 0; i < 8; i++) {
      if (this->crc & 0x0001) {
        this->crc >>= 1;
        this->crc ^= 0xA001;
      } else {
        this->crc >>= 1;
      }
    }
  }
}

void  ModbusCRC::update(const uint8_t val) {
    this->crc ^= (uint16_t)val;
    for (int i = 0; i < 8; i++) {
        if (this->crc & 0x0001) {
            this->crc >>= 1;
            this->crc ^= 0xA001;
        } else {
            this->crc >>= 1;
        }
    }
}


uint16_t ModbusCRC::getcrc(void) {

    return this->crc;

}   

uint16_t ModbusCRC::crc16_modbus(const uint8_t *buf, size_t len) {

    init();
    update(buf,len);
    return getcrc();

}

ModbusCRC::ModbusCRC( void ){}

