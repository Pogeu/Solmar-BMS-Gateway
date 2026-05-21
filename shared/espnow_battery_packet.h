#ifndef SOLMAR_ESPNOW_BATTERY_PACKET_H
#define SOLMAR_ESPNOW_BATTERY_PACKET_H

#include <stdint.h>

#define ESP_NOW_BATTERY_MAGIC 0x42524D53UL
#define ESP_NOW_BATTERY_PROTOCOL_VERSION 1

enum EspNowBatteryStatusFlags : uint16_t {
  ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE = 1 << 0,
  ESP_NOW_BATTERY_STATUS_CHARGE_IMMEDIATELY = 1 << 1,
  ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE = 1 << 2,
};

enum EspNowBatteryFaultFlags : uint16_t {
  ESP_NOW_BATTERY_FAULT_CELL_VOLTAGE_HIGH = 1 << 0,
  ESP_NOW_BATTERY_FAULT_CELL_VOLTAGE_LOW = 1 << 1,
  ESP_NOW_BATTERY_FAULT_CHARGE_CURRENT_HIGH = 1 << 2,
  ESP_NOW_BATTERY_FAULT_DISCHARGE_CURRENT_HIGH = 1 << 3,
  ESP_NOW_BATTERY_FAULT_BMS_TEMPERATURE_HIGH = 1 << 4,
  ESP_NOW_BATTERY_FAULT_CELL_TEMPERATURE_HIGH = 1 << 5,
  ESP_NOW_BATTERY_FAULT_CELL_TEMPERATURE_LOW = 1 << 6,
};

struct __attribute__((packed)) EspNowBatteryPacket {
  uint32_t magic;
  uint8_t protocolVersion;
  uint8_t deviceId;
  uint16_t packetSize;
  uint32_t sequence;
  uint32_t uptimeMs;
  float voltageV;
  float currentA;
  float packPowerW;
  uint16_t socPercent;
  uint16_t temperatureC;
  uint16_t statusFlags;
  uint16_t faultFlags;
};

#endif // SOLMAR_ESPNOW_BATTERY_PACKET_H
