//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#include "main.h"

#if !SERIAL_ONLY_MODE
#include <PubSubClient.h>
#include <WiFi.h>

#include "espnow_battery.h"
#include "felicity.h"
#include "settings.h"

char *mqtt_buildtopic(char *buffer, int device_id, const char *path, const char *optindex = "")
{
  sprintf(buffer,"%s/%d/%s%s",mqttTopic.c_str(), device_id, path, optindex);
  return buffer;
}

static void mqtt_publish_float(PubSubClient &client, char *topic_buffer, char *value_buffer,
                               int device_id, const char *path, float value, int precision)
{
  mqtt_buildtopic(topic_buffer, device_id, path);
  dtostrf(value, 3, precision, value_buffer);
  client.publish(topic_buffer, value_buffer);
}

static void mqtt_publish_int(PubSubClient &client, char *topic_buffer, char *value_buffer,
                             int device_id, const char *path, int value)
{
  mqtt_buildtopic(topic_buffer, device_id, path);
  itoa(value, value_buffer, 10);
  client.publish(topic_buffer, value_buffer);
}

static void mqtt_publish_bool(PubSubClient &client, char *topic_buffer, int device_id,
                              const char *path, bool value)
{
  mqtt_buildtopic(topic_buffer, device_id, path);
  client.publish(topic_buffer, value ? "true" : "false");
}

void mqtt_task(void *param) {
  WiFiClient client;
  PubSubClient mqttclient(client);

  char mqttbuff[256];
  char valuebuff[32];
  char indexbuff[8];
  char mqttClientId[80];
  BmsMessage msg;

  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  sprintf(mqttClientId, "%s-%06X", mqttDevicename.c_str(), chipId);

  mqttclient.setServer(mqttServer.c_str(), mqttPort);
  writeLog("[MQTT] MQTT server configured\n");

  for (;;) {
    if (systemShutdown) {
      Serial.println("[MQTT] Shutting down task...\n");
      vTaskDelete(NULL);
    }

    if (!mqttclient.connected() && mqttServer.length() > 0){
      writeLog("[MQTT] MQTT client state: %d", mqttclient.state());

      if (mqttclient.connect(mqttClientId, mqttUser.c_str(), mqttPass.c_str()) ){
        writeLog("[MQTT] MQTT client connected.\n");
      }
    }

    mqttclient.loop();

    if (xQueueReceive(bmsQueue, &msg, portMAX_DELAY) == pdTRUE) {
      espnowBatteryHandleMessage(msg);

      switch (msg.type) {
        case BMS_TYPE_VERSION_INFO:
            mqtt_publish_int(mqttclient, mqttbuff, valuebuff, msg.deviceId, "version", msg.payload.versionInfo.version);
            break;

        case BMS_TYPE_CELL_VOLTAGES:
            for( uint8_t i = 0; i < msg.payload.cellInfo.validCellCount; i++ ){
              itoa(i, indexbuff, 10);
              mqtt_buildtopic(mqttbuff, msg.deviceId, "cell_voltage_", indexbuff);
              dtostrf(msg.payload.cellInfo.cellVoltages[i], 3, 3, valuebuff);
              mqttclient.publish(mqttbuff, valuebuff);
            }

            for( uint8_t i = 0; i < msg.payload.cellInfo.validTemperatureCount; i++ ){
              itoa(i, indexbuff, 10);
              mqtt_buildtopic(mqttbuff, msg.deviceId, "cell_temperature_", indexbuff);
              dtostrf(msg.payload.cellInfo.cellTemperatures[i], 3, 0, valuebuff);
              mqttclient.publish(mqttbuff, valuebuff);
            }

            mqtt_publish_int(mqttclient, mqttbuff, valuebuff, msg.deviceId, "valid_cell_count", msg.payload.cellInfo.validCellCount);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "cell_min_v", msg.payload.cellInfo.cellMinV, 3);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "cell_max_v", msg.payload.cellInfo.cellMaxV, 3);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "cell_avg_v", msg.payload.cellInfo.cellAvgV, 3);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "cell_sum_v", msg.payload.cellInfo.cellSumV, 3);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "cell_delta_mv", msg.payload.cellInfo.cellDeltaMv, 0);
            mqtt_publish_int(mqttclient, mqttbuff, valuebuff, msg.deviceId, "valid_temperature_count", msg.payload.cellInfo.validTemperatureCount);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "temp_min_c", msg.payload.cellInfo.tempMinC, 1);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "temp_max_c", msg.payload.cellInfo.tempMaxC, 1);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "temp_avg_c", msg.payload.cellInfo.tempAvgC, 1);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "temp_delta_c", msg.payload.cellInfo.tempDeltaC, 1);
            break;

        case BMS_TYPE_CHARGE_DISCHARGE:
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "charge_current_limit", msg.payload.chargeDischarge.chargeCurrentLimit, 1);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "charge_voltage_limit", msg.payload.chargeDischarge.chargeVoltLimit, 2);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "discharge_current_limit", msg.payload.chargeDischarge.dischargeCurrentLimit, 1);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "discharge_voltage_limit", msg.payload.chargeDischarge.dischargeVoltLimit, 2);
            break;

        case BMS_TYPE_BATTERY_INFO:
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "voltage", msg.payload.batteryInfo.voltage, 2);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "current", msg.payload.batteryInfo.current, 2);
            mqtt_publish_float(mqttclient, mqttbuff, valuebuff, msg.deviceId, "pack_power_w", msg.payload.batteryInfo.packPowerW, 1);
            mqtt_publish_int(mqttclient, mqttbuff, valuebuff, msg.deviceId, "soc", msg.payload.batteryInfo.soc);
            mqtt_publish_int(mqttclient, mqttbuff, valuebuff, msg.deviceId, "temperature", msg.payload.batteryInfo.temp);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "battery_charge_enable", msg.payload.batteryInfo.batteryChargeEnable);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "battery_charge_immediately", msg.payload.batteryInfo.batteryChargeImmediately);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "battery_discharge_enable", msg.payload.batteryInfo.batteryDischargeEnable);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_bms_temperature_high", msg.payload.batteryInfo.faultBMSTemperatureHigh);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_cell_temperature_high", msg.payload.batteryInfo.faultCellTemperatureHigh);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_cell_temperature_low", msg.payload.batteryInfo.faultCellTemperatureLow);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_cell_voltage_high", msg.payload.batteryInfo.faultCellVoltageHigh);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_cell_voltage_low", msg.payload.batteryInfo.faultCellVoltageLow);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_charge_current_high", msg.payload.batteryInfo.faultChargeCurrentHigh);
            mqtt_publish_bool(mqttclient, mqttbuff, msg.deviceId, "fault_discharge_current_high", msg.payload.batteryInfo.faultDischargeCurrentHigh);
            break;

        default:
            writeLog("[BMS%d] unknown message type\n", msg.deviceId);
            break;
      }
    }
  }
}
#endif
