//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
// 
// 
#ifndef FELICITY_HTML_H
#define FELICITY_HTML_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;


void startWebServer();
void startConfigPortal();
String getWiFiMqttPage();
String getSerialConfigPage();
String getUpdatePage();

#endif
