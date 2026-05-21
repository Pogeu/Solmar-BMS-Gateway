#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "espnow_battery_packet.h"

#ifndef ESP_NOW_WIFI_CHANNEL
#define ESP_NOW_WIFI_CHANNEL 1
#endif

#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x27
#endif

#ifndef LCD_COLUMNS
#define LCD_COLUMNS 16
#endif

#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif

#ifndef LCD_SDA_PIN
#define LCD_SDA_PIN 8
#endif

#ifndef LCD_SCL_PIN
#define LCD_SCL_PIN 9
#endif

static_assert(sizeof(EspNowBatteryPacket) <= ESP_NOW_MAX_DATA_LEN,
              "ESP-NOW battery packet exceeds maximum payload size");

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

QueueHandle_t packetQueue;
EspNowBatteryPacket latestPacket = {};
uint32_t latestPacketAtMs = 0;
uint32_t latestDisplayUpdateMs = 0;

static void lcdPrintLine(uint8_t row, const char *text)
{
  char padded[LCD_COLUMNS + 1];
  size_t len = strnlen(text, LCD_COLUMNS);

  memset(padded, ' ', LCD_COLUMNS);
  memcpy(padded, text, len);
  padded[LCD_COLUMNS] = '\0';

  lcd.setCursor(0, row);
  lcd.print(padded);
}

static bool packetIsValid(const uint8_t *data, int len)
{
  if (data == nullptr || len != sizeof(EspNowBatteryPacket)) {
    return false;
  }

  const EspNowBatteryPacket *packet = reinterpret_cast<const EspNowBatteryPacket *>(data);

  return packet->magic == ESP_NOW_BATTERY_MAGIC &&
         packet->protocolVersion == ESP_NOW_BATTERY_PROTOCOL_VERSION &&
         packet->packetSize == sizeof(EspNowBatteryPacket);
}

static void onEspNowRecv(const uint8_t *macAddr, const uint8_t *data, int len)
{
  (void)macAddr;

  if (!packetIsValid(data, len)) {
    return;
  }

  EspNowBatteryPacket packet;
  memcpy(&packet, data, sizeof(packet));
  xQueueOverwrite(packetQueue, &packet);
}

static void showStartupScreen()
{
  lcd.clear();
  lcdPrintLine(0, "ESP-NOW BMS RX");
  lcdPrintLine(1, "Aguardando...");
}

static void showWaitingScreen()
{
  lcdPrintLine(0, "ESP-NOW BMS RX");
  lcdPrintLine(1, "Sem dados");
}

static void showBatteryScreen(const EspNowBatteryPacket &packet, bool stale)
{
  char line0[LCD_COLUMNS + 1];
  char line1[LCD_COLUMNS + 1];
  uint8_t page = (millis() / 2500) % 3;

  snprintf(line0, sizeof(line0), "B%u %5.2fV %3u%%",
           packet.deviceId, packet.voltageV, packet.socPercent);

  switch (page) {
    case 0:
      snprintf(line1, sizeof(line1), "%5.1fA %6.1fW",
               packet.currentA, packet.packPowerW);
      break;

    case 1:
      if (packet.faultFlags != 0) {
        snprintf(line1, sizeof(line1), "FALHA 0x%04X", packet.faultFlags);
      } else {
        snprintf(line1, sizeof(line1), "OK Temp %uC", packet.temperatureC);
      }
      break;

    default:
      snprintf(line1, sizeof(line1), "C:%s D:%s %s",
               (packet.statusFlags & ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE) ? "on" : "off",
               (packet.statusFlags & ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE) ? "on" : "off",
               stale ? "OLD" : "NEW");
      break;
  }

  lcdPrintLine(0, line0);
  lcdPrintLine(1, line1);
}

static bool startEspNow()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  esp_err_t result = esp_wifi_set_channel(ESP_NOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (result != ESP_OK) {
    Serial.printf("Failed to set WiFi channel %d: %d\n", ESP_NOW_WIFI_CHANNEL, result);
    return false;
  }

  result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW init failed: %d\n", result);
    return false;
  }

  result = esp_now_register_recv_cb(onEspNowRecv);
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW receive callback failed: %d\n", result);
    return false;
  }

  Serial.printf("ESP-NOW receiver ready on channel %d\n", ESP_NOW_WIFI_CHANNEL);
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  showStartupScreen();

  packetQueue = xQueueCreate(1, sizeof(EspNowBatteryPacket));
  if (packetQueue == nullptr) {
    Serial.println("Failed to create packet queue");
    lcdPrintLine(1, "Erro fila");
    return;
  }

  if (!startEspNow()) {
    lcdPrintLine(0, "ESP-NOW erro");
    lcdPrintLine(1, "Ver Serial");
  }
}

void loop()
{
  EspNowBatteryPacket packet;
  if (packetQueue != nullptr && xQueueReceive(packetQueue, &packet, 0) == pdTRUE) {
    latestPacket = packet;
    latestPacketAtMs = millis();

    Serial.printf("BMS%u V=%.2f I=%.2f P=%.1f SOC=%u T=%u flags=0x%04X faults=0x%04X seq=%lu\n",
                  latestPacket.deviceId,
                  latestPacket.voltageV,
                  latestPacket.currentA,
                  latestPacket.packPowerW,
                  latestPacket.socPercent,
                  latestPacket.temperatureC,
                  latestPacket.statusFlags,
                  latestPacket.faultFlags,
                  (unsigned long)latestPacket.sequence);
  }

  if (millis() - latestDisplayUpdateMs >= 500) {
    latestDisplayUpdateMs = millis();

    if (latestPacketAtMs == 0) {
      showWaitingScreen();
    } else {
      bool stale = millis() - latestPacketAtMs > 10000;
      showBatteryScreen(latestPacket, stale);
    }
  }
}
