//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <string.h>

#include "crc.h"
#include "felicity.h"
#include "main.h"

volatile bool systemShutdown = false;

FelicityBMS::FelicityBMS(int rx, int tx, int num_slaves)
    : FelicityBMS(rx, tx, RS485_DE_PIN, RS485_RE_PIN, num_slaves)
{
}

FelicityBMS::FelicityBMS(int rx, int tx, int de, int re, int num_slaves)
{

    this->dePin = de;
    this->rePin = re;

    pinMode(this->dePin, OUTPUT);
    pinMode(this->rePin, OUTPUT);
    enableRx();

    this->serial = new EspSoftwareSerial::UART(rx, tx, false);
    this->serial->begin(BMS_BAUD_RATE, SWSERIAL_8N1, rx, tx);
    this->num_slaves = num_slaves;
}

FelicityBMS::~FelicityBMS() {
    delete serial;
}

void FelicityBMS::enableTx()
{
    digitalWrite(this->dePin, HIGH);
    digitalWrite(this->rePin, HIGH);
}

void FelicityBMS::enableRx()
{
    digitalWrite(this->dePin, LOW);
    digitalWrite(this->rePin, LOW);
}

static bool isValidCellRaw(uint16_t raw)
{
    return raw != 0x7FFF && raw != 0xFFFF && raw != 0x0000;
}

static bool isValidTemperatureRaw(uint16_t raw)
{
    return raw != 0x7FFF && raw != 0xFFFF;
}

uint16_t FelicityBMS::SendAPDU(uint8_t sid, uint8_t cmd, uint16_t addr, uint16_t len){

    uint8_t buffer[8];
    ModbusCRC crc;

    enableTx();

    // set up frame
    buffer[0] = sid;        // slave ID
    buffer[1] = cmd;        // cmd (default = 03)
    buffer[2] = (addr >> 8);    // addr hi
    buffer[3] = addr & 0xff;    // addr lo
    buffer[4] = (len >> 8);     // len hi
    buffer[5] = len & 0xff;     // len lo

    uint16_t csum = crc.crc16_modbus(buffer, 6);

    // append it to the frame
    buffer[6] = csum & 0xff;
    buffer[7] = (csum >> 8);

#if FELICITY_VERBOSE_MODBUS
    writeLog("send APDU  : %02X %02X %02X %02X %02X %02X %02X %02X\n", buffer[0],
      buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6],
      buffer[7]);
#endif

    // send out and flush
    this->serial->write(buffer, 8);
    this->serial->flush(); 
    delayMicroseconds(100);
    enableRx();

    return 1;
}

/* buffer[len] should be large enough to hold response payload 
   header and crc are stripped off automagically
*/
int FelicityBMS::ReceiveAPDU(uint8_t * buffer, uint16_t len){

    uint8_t header[3];
    uint8_t crcval[2];
    ModbusCRC crc;

    if( len < 2 )
    {
        return -1;
    }

    crc.init();

    // read header (slave id, command, len)
/*
    rs485RxEnable();
    writeLog("waiting for data");
    while( this->serial->available() == 0 ){
      delayMicroseconds(100);
    }
    writeLog("data available");
    for(int i = 0; ; i++ ){
      if (this->serial->available()){
        this->serial->readBytes(header,1);
        writeLog("%02X", header[0]);
      } else {
        return 0;
      }
    }
*/

    this->serial->readBytes(header, 3);
    crc.update(header, 3);

#if FELICITY_VERBOSE_MODBUS
    writeLog("recv header: %02X %02X %02X\n", header[0], header[1], header[2]);
#endif

    // read data
    uint8_t rlen = header[2];
    if (len < rlen) {
        return -2;
    }
    this->serial->readBytes(buffer, rlen);
    crc.update(buffer, rlen);

#if FELICITY_VERBOSE_MODBUS
    Serial.print("recv data  : ");
    for( int i = 0; i < rlen; i++)
    {
      writeLog("%02X ",buffer[i]);
    }
    Serial.println("\n");
#endif

    // read crc
    this->serial->readBytes(crcval, 2);

    uint16_t csum = crc.getcrc();
    
    if ((((uint16_t)crcval[1] << 8) | crcval[0]) != csum ){
        // checksum error
        return -3;
    }

    return rlen;
}

void FelicityBMS::SetQueue(QueueHandle_t bmsq){

  bmsQueue = bmsq;

}

void FelicityBMS::bmsTask(void *param){


  /*  according to Felicity ESS documentation the slaves are 
      numbered sequentially
  */

  uint8_t sid;
  uint8_t * pbuf;
  int rlen;
  uint8_t buffer[64];
  BmsMessage msg;

  // 1x version information inquiry
  for ( int i = 0; i < num_slaves; i++ ){
    sid = (uint8_t)i+1;
    memset(&msg, 0, sizeof(msg));
    msg.deviceId = sid;
    msg.type = BMS_TYPE_VERSION_INFO;
    SendAPDU(sid, 3, 0xF80B, 0x1);
    if( ReceiveAPDU(buffer, 2) == 2){
      msg.payload.versionInfo.version = be16(buffer);
      xQueueSend(bmsQueue, &msg, portMAX_DELAY);
    }
  }

  for (;;) {
    if (systemShutdown) {
      writeLog("[BMS] Shutting down task...\n");      
      vTaskDelete(NULL);
    }

    for ( int i = 0; i < num_slaves; i++ ){

      // cell voltage and temperature information inquiry
      vTaskDelay(100 / portTICK_PERIOD_MS);
      sid = (uint8_t)i+1;
      memset(&msg, 0, sizeof(msg));
      msg.deviceId = sid;
      msg.type = BMS_TYPE_CELL_VOLTAGES;
      SendAPDU(sid, 3, 0x132A, 0x18);
      if( ReceiveAPDU(buffer, 0x30) == 0x30 ){
        pbuf = buffer;

        // The register block exposes 16 cell slots and 8 temperature slots.
        // 16S batteries may use every cell slot; 12.8 V/4S batteries commonly
        // return only 4 valid cells and fill unused cell slots with 0x7FFF.
        for( int j = 0; j < FELICITY_MAX_CELL_SLOTS; j++ ){
          uint16_t raw = be16(pbuf);
          if (isValidCellRaw(raw)) {
            float voltage = raw * 0.001f;
            uint8_t idx = msg.payload.cellInfo.validCellCount++;
            msg.payload.cellInfo.cellVoltages[idx] = voltage;
            msg.payload.cellInfo.cellSumV += voltage;

            if (idx == 0) {
              msg.payload.cellInfo.cellMinV = voltage;
              msg.payload.cellInfo.cellMaxV = voltage;
            } else {
              msg.payload.cellInfo.cellMinV = min(msg.payload.cellInfo.cellMinV, voltage);
              msg.payload.cellInfo.cellMaxV = max(msg.payload.cellInfo.cellMaxV, voltage);
            }
          }
          pbuf += 2;
        }

        if (msg.payload.cellInfo.validCellCount > 0) {
          msg.payload.cellInfo.cellAvgV = msg.payload.cellInfo.cellSumV / msg.payload.cellInfo.validCellCount;
          msg.payload.cellInfo.cellDeltaMv = (msg.payload.cellInfo.cellMaxV - msg.payload.cellInfo.cellMinV) * 1000.0f;
        }

        for( int j = 0; j < FELICITY_MAX_TEMPERATURE_SLOTS; j++ ){
          uint16_t raw = be16(pbuf);
          if (isValidTemperatureRaw(raw)) {
            float temp = (int16_t)raw;
            uint8_t idx = msg.payload.cellInfo.validTemperatureCount++;
            msg.payload.cellInfo.cellTemperatures[idx] = temp;
            msg.payload.cellInfo.tempAvgC += temp;

            if (idx == 0) {
              msg.payload.cellInfo.tempMinC = temp;
              msg.payload.cellInfo.tempMaxC = temp;
            } else {
              msg.payload.cellInfo.tempMinC = min(msg.payload.cellInfo.tempMinC, temp);
              msg.payload.cellInfo.tempMaxC = max(msg.payload.cellInfo.tempMaxC, temp);
            }
          }
          pbuf += 2;
        }

        if (msg.payload.cellInfo.validTemperatureCount > 0) {
          msg.payload.cellInfo.tempAvgC = msg.payload.cellInfo.tempAvgC / msg.payload.cellInfo.validTemperatureCount;
          msg.payload.cellInfo.tempDeltaC = msg.payload.cellInfo.tempMaxC - msg.payload.cellInfo.tempMinC;
        }

        xQueueSend(bmsQueue, &msg, portMAX_DELAY);
      }

      // charger and discharge information inquiry
      vTaskDelay(100 / portTICK_PERIOD_MS);
      memset(&msg, 0, sizeof(msg));
      msg.deviceId = sid;
      msg.type = BMS_TYPE_CHARGE_DISCHARGE;
      SendAPDU(sid, 3, 0x131C, 0x04);
      if( ReceiveAPDU(buffer, 0x08) == 0x08 ){
        msg.payload.chargeDischarge.chargeVoltLimit = be16(&buffer[0]) * 0.01f;
        msg.payload.chargeDischarge.dischargeVoltLimit = be16(&buffer[2]) * 0.01f;
        msg.payload.chargeDischarge.chargeCurrentLimit = be16(&buffer[4]) * 0.1f;
        msg.payload.chargeDischarge.dischargeCurrentLimit = be16(&buffer[6]) * 0.1f;
        xQueueSend(bmsQueue, &msg, portMAX_DELAY);
      }

      // battery information inquiry
      vTaskDelay(100 / portTICK_PERIOD_MS);
      memset(&msg, 0, sizeof(msg));
      msg.deviceId = sid;
      msg.type = BMS_TYPE_BATTERY_INFO;
      SendAPDU(sid, 3, 0x1302, 0x0A);
      if( ReceiveAPDU(buffer, 0x14) == 0x14 ){
        msg.payload.batteryInfo.batteryChargeEnable = (buffer[1]) & 1;
        msg.payload.batteryInfo.batteryChargeImmediately = (buffer[1] >> 1) & 1;
        msg.payload.batteryInfo.batteryDischargeEnable = (buffer[1] >> 2) & 1;
        msg.payload.batteryInfo.faultCellVoltageHigh = (buffer[5] >> 2) & 1;
        msg.payload.batteryInfo.faultCellVoltageLow = (buffer[5] >> 3) & 1;
        msg.payload.batteryInfo.faultChargeCurrentHigh = (buffer[5] >> 4) & 1;
        msg.payload.batteryInfo.faultDischargeCurrentHigh = (buffer[5] >> 5) & 1;
        msg.payload.batteryInfo.faultBMSTemperatureHigh = (buffer[5] >> 6) & 1;
        msg.payload.batteryInfo.faultCellTemperatureHigh = (buffer[4]) & 1;
        msg.payload.batteryInfo.faultCellTemperatureLow = (buffer[4] >> 1) & 1;
        msg.payload.batteryInfo.voltage = be16(&buffer[8]) * 0.01f;
        msg.payload.batteryInfo.current = (int16_t)be16(&buffer[10]) * 0.1f;
        msg.payload.batteryInfo.packPowerW = msg.payload.batteryInfo.voltage * msg.payload.batteryInfo.current;
        msg.payload.batteryInfo.temp = be16(&buffer[16]);
        msg.payload.batteryInfo.soc = be16(&buffer[18]);
        xQueueSend(bmsQueue, &msg, portMAX_DELAY);
      }
    } 
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
