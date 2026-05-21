//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#include "main.h"
#include "settings.h"

Preferences prefs;

String ssid, password, mqttServer, mqttUser, mqttPass, mqttTopic, mqttDevicename;
int rxPin, txPin, rtsPin, mqttPort, batteryCount;

void loadSettings() {
  prefs.begin("config", true);  // read-only
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  mqttServer = prefs.getString("mqtt_server", prefs.getString("mqtt", ""));
  mqttPort = prefs.getInt("mqtt_port", 1883);
  mqttTopic = prefs.getString("mqtt_topic", "bms");
  mqttUser = prefs.getString("mqtt_user", "");
  mqttPass = prefs.getString("mqtt_pass", "");
  mqttDevicename = prefs.getString("mqtt_devicename", prefs.getString("device_name", "felicity2mqtt"));
  rxPin = prefs.getInt("rx_pin", RS485_RX_PIN);
  txPin = prefs.getInt("tx_pin", RS485_TX_PIN);
  rtsPin = prefs.getInt("rts_pin", RS485_DE_PIN);
  batteryCount = prefs.getInt("batt_count", 1);
  prefs.end();
}
