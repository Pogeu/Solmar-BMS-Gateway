//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#ifndef FELICITY_SETTINGS_H
#define FELICITY_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

extern String ssid, password, mqttServer, mqttUser, mqttPass, mqttTopic, mqttDevicename;
extern int rxPin, txPin, rtsPin, batteryCount, mqttPort;
extern Preferences prefs;

void loadSettings();

#endif
