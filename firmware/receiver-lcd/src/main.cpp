#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "espnow_battery_packet.h"

#ifndef ESP_NOW_WIFI_CHANNEL
#define ESP_NOW_WIFI_CHANNEL 1
#endif

#ifndef DISPLAY_SPI_SCK_PIN
#define DISPLAY_SPI_SCK_PIN 18
#endif

#ifndef DISPLAY_SPI_MOSI_PIN
#define DISPLAY_SPI_MOSI_PIN 23
#endif

#ifndef DISPLAY_CS_PIN
#define DISPLAY_CS_PIN 15
#endif

#ifndef DISPLAY_DC_PIN
#define DISPLAY_DC_PIN 16
#endif

#ifndef DISPLAY_RESET_PIN
#define DISPLAY_RESET_PIN 17
#endif

#ifndef DISPLAY_PAGE_BUTTON_PIN
#define DISPLAY_PAGE_BUTTON_PIN 4
#endif

#ifndef DISPLAY_BUTTON_DEBOUNCE_MS
#define DISPLAY_BUTTON_DEBOUNCE_MS 80
#endif

constexpr uint32_t DISPLAY_REFRESH_INTERVAL_MS = 500;
constexpr uint32_t PACKET_STALE_AFTER_MS = 10000;
constexpr uint8_t DISPLAY_PAGE_COUNT = 5;
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_BAR_X = 6;
constexpr uint8_t DISPLAY_BAR_Y = 52;
constexpr uint8_t DISPLAY_BAR_WIDTH = 116;
constexpr uint8_t DISPLAY_BAR_HEIGHT = 8;

#if defined(LCD_STANDALONE_TEST)
constexpr uint32_t DEMO_PACKET_INTERVAL_MS = 1000;
#endif

static_assert(sizeof(EspNowBatteryPacket) <= ESP_NOW_MAX_DATA_LEN,
              "ESP-NOW battery packet exceeds maximum payload size");

U8G2_ST7565_ERC12864_F_4W_SW_SPI display(
    U8G2_R0,
    DISPLAY_SPI_SCK_PIN,
    DISPLAY_SPI_MOSI_PIN,
    DISPLAY_CS_PIN,
    DISPLAY_DC_PIN,
    DISPLAY_RESET_PIN);

QueueHandle_t packetQueue;
EspNowBatteryPacket latestPacket = {};
uint32_t latestPacketAtMs = 0;
uint32_t latestDisplayUpdateMs = 0;
uint8_t displayPage = 0;
volatile bool pageButtonInterruptSeen = false;
bool pageButtonStableState = HIGH;
uint32_t pageButtonLastChangeMs = 0;

#if defined(LCD_STANDALONE_TEST)
uint32_t demoPacketUpdateMs = 0;
uint32_t demoSequence = 0;
#endif

struct VoltageSocPoint {
  float voltage;
  uint8_t socPercent;
};

// Approximate 48 V LiFePO4 resting voltage chart from Jackery.
const VoltageSocPoint LIFEPO4_48V_SOC_TABLE[] = {
  {54.40f, 100},
  {53.60f, 90},
  {53.12f, 80},
  {52.80f, 70},
  {52.32f, 60},
  {52.16f, 50},
  {52.00f, 40},
  {51.52f, 30},
  {51.20f, 20},
  {48.00f, 10},
  {40.00f, 0},
};

static void IRAM_ATTR onPageButtonInterrupt()
{
  pageButtonInterruptSeen = true;
}

static const char *powerUnitText(char unit)
{
  return unit == 'k' ? "kW" : "W";
}

static void formatPageStatus(char *text, size_t textSize, uint8_t pageIndex, bool stale)
{
  snprintf(text, textSize, stale ? "OLD %u/%u" : "%u/%u",
           (unsigned)(pageIndex + 1),
           (unsigned)DISPLAY_PAGE_COUNT);
}

static void drawHeader(const char *title, const char *status)
{
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 8, title);

  if (status != nullptr && status[0] != '\0') {
    int16_t statusX = DISPLAY_WIDTH - (int16_t)display.getStrWidth(status);
    display.drawStr(statusX > 0 ? statusX : 0, 8, status);
  }

  display.drawHLine(0, 10, DISPLAY_WIDTH);
}

static void drawProgressBar(uint8_t percent)
{
  uint8_t cappedPercent = percent > 100 ? 100 : percent;
  uint8_t fillWidth = (uint8_t)(((DISPLAY_BAR_WIDTH - 2) * cappedPercent) / 100);

  display.drawFrame(DISPLAY_BAR_X, DISPLAY_BAR_Y, DISPLAY_BAR_WIDTH, DISPLAY_BAR_HEIGHT);

  if (fillWidth > 0) {
    display.drawBox(DISPLAY_BAR_X + 1, DISPLAY_BAR_Y + 1, fillWidth, DISPLAY_BAR_HEIGHT - 2);
  }
}

static void showTextPage(const char *title,
                         const char *status,
                         const char *line0,
                         const char *line1,
                         const char *line2,
                         const char *line3)
{
  display.clearBuffer();
  drawHeader(title, status);
  display.setFont(u8g2_font_6x10_tf);

  if (line0 != nullptr && line0[0] != '\0') {
    display.drawStr(0, 22, line0);
  }
  if (line1 != nullptr && line1[0] != '\0') {
    display.drawStr(0, 34, line1);
  }
  if (line2 != nullptr && line2[0] != '\0') {
    display.drawStr(0, 46, line2);
  }
  if (line3 != nullptr && line3[0] != '\0') {
    display.drawStr(0, 58, line3);
  }

  display.sendBuffer();
}

static const char *onOffText(bool enabled)
{
  return enabled ? "ON" : "OFF";
}

static uint32_t cappedAgeSeconds(uint32_t ageMs)
{
  uint32_t ageSeconds = ageMs / 1000;
  return ageSeconds > 999 ? 999 : ageSeconds;
}

static uint16_t cappedSocPercent(uint16_t socPercent)
{
  return socPercent > 100 ? 100 : socPercent;
}

static uint8_t lifepo4SocFromRestingVoltage(float voltage)
{
  const size_t pointCount = sizeof(LIFEPO4_48V_SOC_TABLE) / sizeof(LIFEPO4_48V_SOC_TABLE[0]);

  if (voltage >= LIFEPO4_48V_SOC_TABLE[0].voltage) {
    return LIFEPO4_48V_SOC_TABLE[0].socPercent;
  }

  for (size_t i = 1; i < pointCount; i++) {
    const VoltageSocPoint &high = LIFEPO4_48V_SOC_TABLE[i - 1];
    const VoltageSocPoint &low = LIFEPO4_48V_SOC_TABLE[i];

    if (voltage <= high.voltage && voltage >= low.voltage) {
      float voltageSpan = high.voltage - low.voltage;
      float ratio = voltageSpan > 0.0f ? (voltage - low.voltage) / voltageSpan : 0.0f;
      return low.socPercent + (uint8_t)roundf(ratio * (high.socPercent - low.socPercent));
    }
  }

  return LIFEPO4_48V_SOC_TABLE[pointCount - 1].socPercent;
}

static long roundedWatts(float watts)
{
  return watts >= 0.0f ? (long)(watts + 0.5f) : (long)(watts - 0.5f);
}

static long displayWatts(float watts)
{
  long rounded = roundedWatts(watts);

  if (rounded > 9999) {
    rounded = 9999;
  } else if (rounded < -9999) {
    rounded = -9999;
  }

  return rounded;
}

static char formatBigPower(float watts, char *text, size_t textSize)
{
  long rounded = roundedWatts(watts);

  if (rounded >= 1000 || rounded <= -1000) {
    float kilowatts = watts / 1000.0f;

    if (kilowatts > 9.9f) {
      kilowatts = 9.9f;
    } else if (kilowatts < -9.0f) {
      kilowatts = -9.0f;
    }

    snprintf(text, textSize, "%3.1f", kilowatts);
    return 'k';
  }

  if (rounded > 999) {
    rounded = 999;
  } else if (rounded < -99) {
    rounded = -99;
  }

  snprintf(text, textSize, "%3ld", rounded);
  return 'W';
}

static void setupPageButton()
{
  pinMode(DISPLAY_PAGE_BUTTON_PIN, INPUT_PULLUP);
  pageButtonStableState = digitalRead(DISPLAY_PAGE_BUTTON_PIN);
  attachInterrupt(digitalPinToInterrupt(DISPLAY_PAGE_BUTTON_PIN), onPageButtonInterrupt, CHANGE);
}

static void forceDisplayRefresh()
{
  latestDisplayUpdateMs = 0;
}

static void pollPageButton()
{
  if (!pageButtonInterruptSeen) {
    return;
  }

  noInterrupts();
  pageButtonInterruptSeen = false;
  interrupts();

  bool reading = digitalRead(DISPLAY_PAGE_BUTTON_PIN);
  uint32_t now = millis();

  if (now - pageButtonLastChangeMs < DISPLAY_BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (reading == pageButtonStableState) {
    return;
  }

  pageButtonStableState = reading;
  pageButtonLastChangeMs = now;

  if (pageButtonStableState == LOW) {
    displayPage = (displayPage + 1) % DISPLAY_PAGE_COUNT;
    forceDisplayRefresh();
  }
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
  showTextPage("ESP-NOW RX", "SPI", "Display 128x64", "ST7565 pronto", "Aguardando dados", "");
}

static void showWaitingScreen()
{
  showTextPage("ESP-NOW RX", "SPI", "Sem pacotes", "Confira canal", "Aguardando gateway", "");
}

#if defined(LCD_STANDALONE_TEST)
static EspNowBatteryPacket makeDemoPacket(uint32_t sequence)
{
  EspNowBatteryPacket packet = {};
  uint8_t phase = sequence % 24;
  uint16_t soc = phase < 12 ? 35 + phase : 47 - (phase - 12);
  float current = 2.06f + (phase % 5) * 0.18f;
  float voltage = 52.4f + (phase % 4) * 0.1f;

  packet.magic = ESP_NOW_BATTERY_MAGIC;
  packet.protocolVersion = ESP_NOW_BATTERY_PROTOCOL_VERSION;
  packet.packetSize = sizeof(EspNowBatteryPacket);
  packet.deviceId = 1;
  packet.sequence = sequence;
  packet.uptimeMs = millis();
  packet.voltageV = voltage;
  packet.currentA = current;
  packet.packPowerW = voltage * current;
  packet.socPercent = soc;
  packet.temperatureC = 22 + (phase % 4);
  packet.statusFlags = ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE |
                       ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE;
  packet.faultFlags = (phase >= 18) ? ESP_NOW_BATTERY_FAULT_BMS_TEMPERATURE_HIGH : 0;

  return packet;
}

static void updateDemoPacket()
{
  uint32_t now = millis();

  if (latestPacketAtMs != 0 && now - demoPacketUpdateMs < DEMO_PACKET_INTERVAL_MS) {
    return;
  }

  demoPacketUpdateMs = now;
  latestPacket = makeDemoPacket(demoSequence++);
  latestPacketAtMs = now;

  Serial.printf("LCD demo BMS%u V=%.2f I=%.2f P=%.1f SOC=%u T=%u faults=0x%04X seq=%lu\n",
                latestPacket.deviceId,
                latestPacket.voltageV,
                latestPacket.currentA,
                latestPacket.packPowerW,
                latestPacket.socPercent,
                latestPacket.temperatureC,
                latestPacket.faultFlags,
                (unsigned long)latestPacket.sequence);
}
#endif

static void showMainPage(const EspNowBatteryPacket &packet, bool stale)
{
  char socText[4];
  char powerText[5];
  char footer[24];
  char status[12];
  uint16_t socPercent = cappedSocPercent(packet.socPercent);
  char powerUnit = formatBigPower(packet.packPowerW, powerText, sizeof(powerText));

  snprintf(socText, sizeof(socText), "%u", socPercent);
  snprintf(footer, sizeof(footer), "PWR %s %s", powerText, powerUnitText(powerUnit));
  formatPageStatus(status, sizeof(status), 0, stale);

  display.clearBuffer();
  drawHeader("ESP-NOW RX", status);

  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 22, "SOC");
  display.drawStr(72, 22, "PWR");

  display.setFont(u8g2_font_logisoso18_tn);
  display.drawStr(0, 47, socText);
  display.drawStr(68, 47, powerText);

  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(44, 46, "%");
  display.drawStr(110, 46, powerUnitText(powerUnit));
  drawProgressBar((uint8_t)socPercent);
  display.drawStr(0, 63, footer);
  display.sendBuffer();
}

static void showElectricalPage(const EspNowBatteryPacket &packet)
{
  char line0[24];
  char line1[24];
  char line2[24];
  char line3[24];
  char status[12];
  uint16_t socPercent = cappedSocPercent(packet.socPercent);

  formatPageStatus(status, sizeof(status), 1, latestPacketAtMs != 0 && (millis() - latestPacketAtMs > PACKET_STALE_AFTER_MS));
  snprintf(line0, sizeof(line0), "Volt  %5.2f V", packet.voltageV);
  snprintf(line1, sizeof(line1), "Corr  %5.1f A", packet.currentA);
  snprintf(line2, sizeof(line2), "Pot   %5ld W", displayWatts(packet.packPowerW));
  snprintf(line3, sizeof(line3), "SOC   %3u %%", socPercent);

  showTextPage("Eletrica", status, line0, line1, line2, line3);
}

static void showLifepo4SocPage(const EspNowBatteryPacket &packet)
{
  char line0[24];
  char line1[24];
  char line2[24];
  char line3[24];
  char status[12];
  uint8_t voltageSoc = lifepo4SocFromRestingVoltage(packet.voltageV);
  uint16_t bmsSoc = cappedSocPercent(packet.socPercent);

  formatPageStatus(status, sizeof(status), 2, latestPacketAtMs != 0 && (millis() - latestPacketAtMs > PACKET_STALE_AFTER_MS));
  snprintf(line0, sizeof(line0), "BMS SOC %3u %%", bmsSoc);
  snprintf(line1, sizeof(line1), "VoltSOC %3u %%", voltageSoc);
  snprintf(line2, sizeof(line2), "Pack    %5.2f V", packet.voltageV);
  snprintf(line3, sizeof(line3), "OCV so em repouso");

  showTextPage("SOC 48V", status, line0, line1, line2, line3);
}

static void showStatusPage(const EspNowBatteryPacket &packet)
{
  char line0[24];
  char line1[24];
  char line2[24];
  char line3[24];
  char status[12];

  formatPageStatus(status, sizeof(status), 3, latestPacketAtMs != 0 && (millis() - latestPacketAtMs > PACKET_STALE_AFTER_MS));
  snprintf(line0, sizeof(line0), "BMS ID %u", packet.deviceId);
  snprintf(line1, sizeof(line1), "Temp   %3u C", packet.temperatureC);
  snprintf(line2, sizeof(line2), "Charge %s",
           onOffText(packet.statusFlags & ESP_NOW_BATTERY_STATUS_CHARGE_ENABLE));
  snprintf(line3, sizeof(line3), "Dischg %s",
           onOffText(packet.statusFlags & ESP_NOW_BATTERY_STATUS_DISCHARGE_ENABLE));

  showTextPage("Status", status, line0, line1, line2, line3);
}

static void showFaultLinkPage(const EspNowBatteryPacket &packet, uint32_t ageMs, bool stale)
{
  char line0[24];
  char line1[24];
  char line2[24];
  char line3[24];
  char status[12];

  formatPageStatus(status, sizeof(status), 4, stale);

  if (packet.faultFlags != 0) {
    snprintf(line0, sizeof(line0), "Falha 0x%04X", packet.faultFlags);
  } else {
    snprintf(line0, sizeof(line0), "Sem falhas");
  }

  snprintf(line1, sizeof(line1), "Seq %lu", (unsigned long)packet.sequence);
  snprintf(line2, sizeof(line2), "Tensao %5.2f V", packet.voltageV);
  snprintf(line3, sizeof(line3), "Link %s %3lus",
           stale ? "OLD" : "OK ",
           (unsigned long)cappedAgeSeconds(ageMs));

  showTextPage("Falhas", status, line0, line1, line2, line3);
}

static void showBatteryScreen(const EspNowBatteryPacket &packet, uint32_t ageMs)
{
  bool stale = ageMs > PACKET_STALE_AFTER_MS;

  switch (displayPage) {
    case 0:
      showMainPage(packet, stale);
      break;

    case 1:
      showElectricalPage(packet);
      break;

    case 2:
      showLifepo4SocPage(packet);
      break;

    case 3:
      showStatusPage(packet);
      break;

    default:
      showFaultLinkPage(packet, ageMs, stale);
      break;
  }
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

  display.begin();
  setupPageButton();
  showStartupScreen();

#if defined(LCD_STANDALONE_TEST)
  Serial.println("Display standalone test mode");
  showTextPage("Teste local", "SPI", "Paginas do display", "Botao troca pag", "Sem ESP-NOW", "");
#else
  packetQueue = xQueueCreate(1, sizeof(EspNowBatteryPacket));
  if (packetQueue == nullptr) {
    Serial.println("Failed to create packet queue");
    showTextPage("ESP-NOW RX", "ERRO", "Falha na fila", "Ver Serial", "", "");
    return;
  }

  if (!startEspNow()) {
    showTextPage("ESP-NOW RX", "ERRO", "Falha ESP-NOW", "Ver Serial", "", "");
  }
#endif
}

void loop()
{
  pollPageButton();

#if defined(LCD_STANDALONE_TEST)
  updateDemoPacket();
#else
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
#endif

  if (millis() - latestDisplayUpdateMs >= DISPLAY_REFRESH_INTERVAL_MS) {
    latestDisplayUpdateMs = millis();

    if (latestPacketAtMs == 0) {
      showWaitingScreen();
    } else {
      showBatteryScreen(latestPacket, millis() - latestPacketAtMs);
    }
  }
}
