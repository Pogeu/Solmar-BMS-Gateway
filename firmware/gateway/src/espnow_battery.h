#ifndef FELICITY_ESPNOW_BATTERY_H
#define FELICITY_ESPNOW_BATTERY_H

#include <Arduino.h>

#include "espnow_battery_packet.h"
#include "felicity.h"

#ifndef ESP_NOW_ENABLE
#define ESP_NOW_ENABLE 1
#endif

#ifndef ESP_NOW_WIFI_CHANNEL
#define ESP_NOW_WIFI_CHANNEL 1
#endif

inline uint16_t espnowBatteryStatusFlags(const BmsMessage &msg)
{
  uint16_t flags = 0;

  if (msg.payload.batteryInfo.batteryChargeEnable) {
    flags |= ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE;
  }
  if (msg.payload.batteryInfo.batteryChargeImmediately) {
    flags |= ESP_NOW_BATTERY_STATUS_CHARGE_IMMEDIATELY;
  }
  if (msg.payload.batteryInfo.batteryDischargeEnable) {
    flags |= ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE;
  }

  return flags;
}

inline uint16_t espnowBatteryFaultFlags(const BmsMessage &msg)
{
  uint16_t flags = 0;

  if (msg.payload.batteryInfo.faultCellVoltageHigh) {
    flags |= ESP_NOW_BATTERY_FAULT_CELL_VOLTAGE_HIGH;
  }
  if (msg.payload.batteryInfo.faultCellVoltageLow) {
    flags |= ESP_NOW_BATTERY_FAULT_CELL_VOLTAGE_LOW;
  }
  if (msg.payload.batteryInfo.faultChargeCurrentHigh) {
    flags |= ESP_NOW_BATTERY_FAULT_CHARGE_CURRENT_HIGH;
  }
  if (msg.payload.batteryInfo.faultDischargeCurrentHigh) {
    flags |= ESP_NOW_BATTERY_FAULT_DISCHARGE_CURRENT_HIGH;
  }
  if (msg.payload.batteryInfo.faultBMSTemperatureHigh) {
    flags |= ESP_NOW_BATTERY_FAULT_BMS_TEMPERATURE_HIGH;
  }
  if (msg.payload.batteryInfo.faultCellTemperatureHigh) {
    flags |= ESP_NOW_BATTERY_FAULT_CELL_TEMPERATURE_HIGH;
  }
  if (msg.payload.batteryInfo.faultCellTemperatureLow) {
    flags |= ESP_NOW_BATTERY_FAULT_CELL_TEMPERATURE_LOW;
  }

  return flags;
}

inline bool espnowBatteryBuildPacket(const BmsMessage &msg, uint32_t sequence,
                                     uint32_t uptimeMs, EspNowBatteryPacket *packet)
{
  if (packet == nullptr || msg.type != BMS_TYPE_BATTERY_INFO) {
    return false;
  }

  *packet = {};
  packet->magic = ESP_NOW_BATTERY_MAGIC;
  packet->protocolVersion = ESP_NOW_BATTERY_PROTOCOL_VERSION;
  packet->deviceId = msg.deviceId;
  packet->packetSize = sizeof(EspNowBatteryPacket);
  packet->sequence = sequence;
  packet->uptimeMs = uptimeMs;
  packet->voltageV = msg.payload.batteryInfo.voltage;
  packet->currentA = msg.payload.batteryInfo.current;
  packet->packPowerW = msg.payload.batteryInfo.packPowerW;
  packet->socPercent = msg.payload.batteryInfo.soc;
  packet->temperatureC = msg.payload.batteryInfo.temp;
  packet->statusFlags = espnowBatteryStatusFlags(msg);
  packet->faultFlags = espnowBatteryFaultFlags(msg);

  return true;
}

bool espnowBatteryBegin();
bool espnowBatteryHandleMessage(const BmsMessage &msg);
bool espnowBatterySend(const BmsMessage &msg);

#endif // FELICITY_ESPNOW_BATTERY_H
