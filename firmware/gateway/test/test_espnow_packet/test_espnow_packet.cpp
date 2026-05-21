#include <Arduino.h>
#include <unity.h>

#include "../../src/espnow_battery.h"

static BmsMessage makeBatteryMessage()
{
  BmsMessage msg = {};
  msg.deviceId = 3;
  msg.type = BMS_TYPE_BATTERY_INFO;
  msg.payload.batteryInfo.batteryChargeEnable = true;
  msg.payload.batteryInfo.batteryChargeImmediately = false;
  msg.payload.batteryInfo.batteryDischargeEnable = true;
  msg.payload.batteryInfo.faultCellVoltageHigh = true;
  msg.payload.batteryInfo.faultCellVoltageLow = false;
  msg.payload.batteryInfo.faultChargeCurrentHigh = true;
  msg.payload.batteryInfo.faultDischargeCurrentHigh = false;
  msg.payload.batteryInfo.faultBMSTemperatureHigh = true;
  msg.payload.batteryInfo.faultCellTemperatureHigh = false;
  msg.payload.batteryInfo.faultCellTemperatureLow = true;
  msg.payload.batteryInfo.voltage = 51.23f;
  msg.payload.batteryInfo.current = -12.4f;
  msg.payload.batteryInfo.packPowerW = -635.252f;
  msg.payload.batteryInfo.soc = 87;
  msg.payload.batteryInfo.temp = 29;

  return msg;
}

void test_builds_espnow_battery_packet()
{
  BmsMessage msg = makeBatteryMessage();
  EspNowBatteryPacket packet;

  TEST_ASSERT_TRUE(espnowBatteryBuildPacket(msg, 42, 123456, &packet));
  TEST_ASSERT_EQUAL_UINT32(ESP_NOW_BATTERY_MAGIC, packet.magic);
  TEST_ASSERT_EQUAL_UINT8(ESP_NOW_BATTERY_PROTOCOL_VERSION, packet.protocolVersion);
  TEST_ASSERT_EQUAL_UINT8(3, packet.deviceId);
  TEST_ASSERT_EQUAL_UINT16(sizeof(EspNowBatteryPacket), packet.packetSize);
  TEST_ASSERT_EQUAL_UINT32(42, packet.sequence);
  TEST_ASSERT_EQUAL_UINT32(123456, packet.uptimeMs);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 51.23f, packet.voltageV);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.4f, packet.currentA);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -635.252f, packet.packPowerW);
  TEST_ASSERT_EQUAL_UINT16(87, packet.socPercent);
  TEST_ASSERT_EQUAL_UINT16(29, packet.temperatureC);
}

void test_sets_status_and_fault_flags()
{
  BmsMessage msg = makeBatteryMessage();
  EspNowBatteryPacket packet;

  TEST_ASSERT_TRUE(espnowBatteryBuildPacket(msg, 1, 2, &packet));

  uint16_t expectedStatusFlags = ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE |
                                 ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE;
  uint16_t expectedFaultFlags = ESP_NOW_BATTERY_FAULT_CELL_VOLTAGE_HIGH |
                                ESP_NOW_BATTERY_FAULT_CHARGE_CURRENT_HIGH |
                                ESP_NOW_BATTERY_FAULT_BMS_TEMPERATURE_HIGH |
                                ESP_NOW_BATTERY_FAULT_CELL_TEMPERATURE_LOW;

  TEST_ASSERT_EQUAL_UINT16(expectedStatusFlags, packet.statusFlags);
  TEST_ASSERT_EQUAL_UINT16(expectedFaultFlags, packet.faultFlags);
}

void test_rejects_non_battery_messages()
{
  BmsMessage msg = {};
  msg.deviceId = 1;
  msg.type = BMS_TYPE_CELL_VOLTAGES;
  EspNowBatteryPacket packet;

  TEST_ASSERT_FALSE(espnowBatteryBuildPacket(msg, 1, 2, &packet));
  TEST_ASSERT_FALSE(espnowBatteryBuildPacket(msg, 1, 2, nullptr));
}

void setup()
{
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_builds_espnow_battery_packet);
  RUN_TEST(test_sets_status_and_fault_flags);
  RUN_TEST(test_rejects_non_battery_messages);
  UNITY_END();
}

void loop()
{
}
