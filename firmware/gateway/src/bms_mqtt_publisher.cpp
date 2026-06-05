#include "bms_mqtt_publisher.h"

#ifndef BMS_MQTT_ENABLE
#define BMS_MQTT_ENABLE 0
#endif

#if BMS_MQTT_ENABLE

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <string.h>

#include "bms_telemetry_json.h"
#include "main.h"

#ifndef BMS_MQTT_HOST
#define BMS_MQTT_HOST "broker.hivemq.com"
#endif

#ifndef BMS_MQTT_PORT
#define BMS_MQTT_PORT 1883
#endif

#ifndef BMS_MQTT_TOPIC_BASE
#define BMS_MQTT_TOPIC_BASE "solmar/bms/felicity-fla12171"
#endif

#ifndef BMS_MQTT_CLIENT_PREFIX
#define BMS_MQTT_CLIENT_PREFIX "solmar-bms-gateway"
#endif

#ifndef BMS_MQTT_USERNAME
#define BMS_MQTT_USERNAME ""
#endif

#ifndef BMS_MQTT_PASSWORD
#define BMS_MQTT_PASSWORD ""
#endif

#ifndef BMS_MQTT_RETAIN
#define BMS_MQTT_RETAIN 1
#endif

#ifndef BMS_MQTT_BUFFER_SIZE
#define BMS_MQTT_BUFFER_SIZE 1536
#endif

#ifndef BMS_MQTT_TOPIC_BUFFER_SIZE
#define BMS_MQTT_TOPIC_BUFFER_SIZE 128
#endif

#ifndef BMS_MQTT_RECONNECT_INTERVAL_MS
#define BMS_MQTT_RECONNECT_INTERVAL_MS 5000
#endif

#ifndef BMS_WIFI_RECONNECT_INTERVAL_MS
#define BMS_WIFI_RECONNECT_INTERVAL_MS 15000
#endif

#ifndef BMS_WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS
#define BMS_WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS 120
#endif

#ifndef BMS_WIFI_CONNECT_TIMEOUT_SECONDS
#define BMS_WIFI_CONNECT_TIMEOUT_SECONDS 20
#endif

#ifndef BMS_WIFI_MANAGER_DEBUG
#define BMS_WIFI_MANAGER_DEBUG 0
#endif

#ifndef BMS_WIFI_AP_NAME
#define BMS_WIFI_AP_NAME "Solmar-BMS-Setup"
#endif

#ifndef BMS_WIFI_AP_PASSWORD
#define BMS_WIFI_AP_PASSWORD ""
#endif

class BmsNetworkTransport {
public:
  bool begin()
  {
    if (started_) {
      return connected();
    }

    started_ = true;
    WiFi.mode(WIFI_STA);
    manager_.setDebugOutput(BMS_WIFI_MANAGER_DEBUG);
    manager_.setConfigPortalTimeout(BMS_WIFI_CONFIG_PORTAL_TIMEOUT_SECONDS);
    manager_.setConnectTimeout(BMS_WIFI_CONNECT_TIMEOUT_SECONDS);

    const char *portalPassword = strlen(BMS_WIFI_AP_PASSWORD) > 0 ? BMS_WIFI_AP_PASSWORD : nullptr;
    bool ok = manager_.autoConnect(BMS_WIFI_AP_NAME, portalPassword);

    if (ok) {
      writeLog("[NET] WiFi connected: %s RSSI=%d dBm\n",
               WiFi.localIP().toString().c_str(),
               WiFi.RSSI());
    } else {
      writeLog("[NET] WiFi not connected; MQTT will retry in the background\n");
    }

    return ok;
  }

  void loop()
  {
    if (connected()) {
      return;
    }

    uint32_t now = millis();
    if (now - latestReconnectAttemptMs_ < BMS_WIFI_RECONNECT_INTERVAL_MS) {
      return;
    }

    latestReconnectAttemptMs_ = now;
    writeLog("[NET] WiFi reconnect attempt\n");
    WiFi.reconnect();
  }

  bool connected() const
  {
    return WiFi.status() == WL_CONNECTED;
  }

  Client &client()
  {
    return wifiClient_;
  }

  const char *name() const
  {
    return "wifi-manager";
  }

private:
  WiFiClient wifiClient_;
  WiFiManager manager_;
  bool started_ = false;
  uint32_t latestReconnectAttemptMs_ = 0;
};

static BmsNetworkTransport networkTransport;
static PubSubClient mqttClient(networkTransport.client());
static uint32_t mqttSequence = 0;
static uint32_t latestMqttReconnectAttemptMs = 0;
static bool mqttStarted = false;
static char mqttPayloadBuffer[BMS_MQTT_BUFFER_SIZE];
static char mqttTopicBuffer[BMS_MQTT_TOPIC_BUFFER_SIZE];

static void buildClientId(char *buffer, size_t bufferSize)
{
#if defined(ESP32)
  snprintf(buffer, bufferSize, "%s-%08lx", BMS_MQTT_CLIENT_PREFIX, (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFFUL));
#else
  snprintf(buffer, bufferSize, "%s-%lu", BMS_MQTT_CLIENT_PREFIX, (unsigned long)millis());
#endif
}

static bool mqttCredentialsConfigured()
{
  return strlen(BMS_MQTT_USERNAME) > 0;
}

static bool connectMqtt()
{
  if (!networkTransport.connected()) {
    return false;
  }

  char clientId[64];
  buildClientId(clientId, sizeof(clientId));
  writeLog("[MQTT] Connecting to %s:%u as %s\n", BMS_MQTT_HOST, (unsigned)BMS_MQTT_PORT, clientId);

  bool connected = false;
  if (mqttCredentialsConfigured()) {
    connected = mqttClient.connect(clientId, BMS_MQTT_USERNAME, BMS_MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId);
  }

  if (!connected) {
    writeLog("[MQTT] Connect failed, state=%d\n", mqttClient.state());
    return false;
  }

  writeLog("[MQTT] Connected, publishing retained readings under %s/readings/v1\n", BMS_MQTT_TOPIC_BASE);
  return true;
}

static bool ensureMqttConnected()
{
  if (!mqttStarted) {
    return false;
  }

  networkTransport.loop();

  if (!networkTransport.connected()) {
    return false;
  }

  if (mqttClient.connected()) {
    return true;
  }

  uint32_t now = millis();
  if (latestMqttReconnectAttemptMs != 0 &&
      now - latestMqttReconnectAttemptMs < BMS_MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }

  latestMqttReconnectAttemptMs = now;
  return connectMqtt();
}

static bool buildTopic(const BmsMessage &msg)
{
  int written = snprintf(mqttTopicBuffer,
                         sizeof(mqttTopicBuffer),
                         "%s/readings/v1/%u/%s",
                         BMS_MQTT_TOPIC_BASE,
                         (unsigned)msg.deviceId,
                         bmsTelemetryMessageTypeName(msg.type));

  return written > 0 && (size_t)written < sizeof(mqttTopicBuffer);
}

bool bmsMqttPublisherBegin()
{
  if (mqttStarted) {
    return bmsMqttPublisherIsReady();
  }

  mqttStarted = true;
  writeLog("[MQTT] Network backend: %s\n", networkTransport.name());
  mqttClient.setServer(BMS_MQTT_HOST, BMS_MQTT_PORT);
  mqttClient.setBufferSize(BMS_MQTT_BUFFER_SIZE);

  networkTransport.begin();
  return ensureMqttConnected();
}

void bmsMqttPublisherLoop()
{
  if (!ensureMqttConnected()) {
    return;
  }

  mqttClient.loop();
}

bool bmsMqttPublisherHandleMessage(const BmsMessage &msg)
{
  if (!ensureMqttConnected()) {
    return false;
  }

  if (!buildTopic(msg)) {
    writeLog("[MQTT] Topic too long for BMS%u %s\n",
             (unsigned)msg.deviceId,
             bmsTelemetryMessageTypeName(msg.type));
    return false;
  }

  if (!bmsTelemetryBuildJson(msg, ++mqttSequence, millis(), mqttPayloadBuffer, sizeof(mqttPayloadBuffer))) {
    writeLog("[MQTT] JSON payload truncated for BMS%u %s\n",
             (unsigned)msg.deviceId,
             bmsTelemetryMessageTypeName(msg.type));
    return false;
  }

  bool ok = mqttClient.publish(mqttTopicBuffer, mqttPayloadBuffer, BMS_MQTT_RETAIN != 0);
  if (!ok) {
    writeLog("[MQTT] Publish failed, topic=%s state=%d\n", mqttTopicBuffer, mqttClient.state());
    return false;
  }

  return true;
}

bool bmsMqttPublisherIsReady()
{
  return mqttStarted && networkTransport.connected() && mqttClient.connected();
}

#else

bool bmsMqttPublisherBegin()
{
  return false;
}

void bmsMqttPublisherLoop()
{
}

bool bmsMqttPublisherHandleMessage(const BmsMessage &msg)
{
  (void)msg;
  return false;
}

bool bmsMqttPublisherIsReady()
{
  return false;
}

#endif
