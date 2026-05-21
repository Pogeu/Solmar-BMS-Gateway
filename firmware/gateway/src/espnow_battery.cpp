#include "espnow_battery.h"

#if ESP_NOW_ENABLE
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "main.h"

static_assert(sizeof(EspNowBatteryPacket) <= ESP_NOW_MAX_DATA_LEN,
              "ESP-NOW battery packet exceeds maximum payload size");

static const uint8_t ESP_NOW_BROADCAST_MAC[6] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static bool espnowInitialized = false;
static uint32_t espnowSequence = 0;

static void onEspNowSent(const uint8_t *macAddr, esp_now_send_status_t status)
{
  (void)macAddr;

  if (status != ESP_NOW_SEND_SUCCESS) {
    writeLog("[ESP-NOW] Battery packet delivery not confirmed\n");
  }
}

static bool setStandaloneWifiChannel()
{
  if (ESP_NOW_WIFI_CHANNEL == 0 || WiFi.status() == WL_CONNECTED) {
    return true;
  }

  esp_err_t result = esp_wifi_set_channel(ESP_NOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    writeLog("[ESP-NOW] Failed to set WiFi channel %d: %d\n", ESP_NOW_WIFI_CHANNEL, result);
    return false;
  }

  return true;
}

bool espnowBatteryBegin()
{
  if (espnowInitialized) {
    return true;
  }

  if (WiFi.getMode() == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
  }

  if (!setStandaloneWifiChannel()) {
    return false;
  }

  esp_err_t result = esp_now_init();
  if (result != ESP_OK) {
    writeLog("[ESP-NOW] Init failed: %d\n", result);
    return false;
  }

  esp_now_register_send_cb(onEspNowSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, ESP_NOW_BROADCAST_MAC, sizeof(ESP_NOW_BROADCAST_MAC));
  peerInfo.channel = WiFi.status() == WL_CONNECTED ? 0 : ESP_NOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(ESP_NOW_BROADCAST_MAC)) {
    result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
      writeLog("[ESP-NOW] Failed to add broadcast peer: %d\n", result);
      return false;
    }
  }

  espnowInitialized = true;
  writeLog("[ESP-NOW] Battery broadcast ready on channel %d\n", peerInfo.channel);
  return true;
}

bool espnowBatterySend(const BmsMessage &msg)
{
  if (msg.type != BMS_TYPE_BATTERY_INFO) {
    return false;
  }

  if (!espnowBatteryBegin()) {
    return false;
  }

  EspNowBatteryPacket packet;
  if (!espnowBatteryBuildPacket(msg, ++espnowSequence, millis(), &packet)) {
    return false;
  }

  esp_err_t result = esp_now_send(ESP_NOW_BROADCAST_MAC, (const uint8_t *)&packet, sizeof(packet));
  if (result != ESP_OK) {
    writeLog("[ESP-NOW] Send failed: %d\n", result);
    return false;
  }

  return true;
}

bool espnowBatteryHandleMessage(const BmsMessage &msg)
{
  if (msg.type != BMS_TYPE_BATTERY_INFO) {
    return false;
  }

  return espnowBatterySend(msg);
}

#else

bool espnowBatteryBegin()
{
  return false;
}

bool espnowBatteryHandleMessage(const BmsMessage &msg)
{
  (void)msg;
  return false;
}

bool espnowBatterySend(const BmsMessage &msg)
{
  (void)msg;
  return false;
}

#endif
