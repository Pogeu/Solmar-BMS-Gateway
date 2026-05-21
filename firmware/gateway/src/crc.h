//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include <Arduino.h>

class ModbusCRC
{
private:
    uint16_t crc;
public:
    ModbusCRC(void);
    void init(void);
    void update(const uint8_t *buf, size_t len);
    void update(const uint8_t value);
    uint16_t crc16_modbus(const uint8_t *buf, size_t len);
    uint16_t getcrc(void);   
};


#endif // MODBUS_CRC_H