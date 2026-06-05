#include "bms_sd_logger.h"

#include "bms_telemetry_json.h"
#include "main.h"

#if SD_LOG_ENABLE

#include <FS.h>
#include <SD.h>
#include <SPI.h>

static File sdLogFile;
static bool sdLoggerStarted = false;
static bool sdLoggerReady = false;
static uint32_t sdLogSequence = 0;

static bool openSdLogFile()
{
  if (sdLogFile) {
    return true;
  }

  sdLogFile = SD.open(SD_LOG_FILE_PATH, FILE_APPEND);
  if (!sdLogFile) {
    writeLog("[SD] Failed to open %s for append\n", SD_LOG_FILE_PATH);
    return false;
  }

  return true;
}

bool bmsSdLoggerBegin()
{
  if (sdLoggerStarted) {
    return sdLoggerReady;
  }

  sdLoggerStarted = true;

  writeLog("[SD] CS pin: GPIO%d\n", SD_LOG_CS_PIN);

  pinMode(SD_LOG_CS_PIN, OUTPUT);
  digitalWrite(SD_LOG_CS_PIN, HIGH);

#if SD_LOG_USE_DEFAULT_SPI_PINS
  writeLog("[SD] SPI bus: default pins\n");
  SPI.begin();
#else
  writeLog("[SD] SPI pins: SCK=%d MISO=%d MOSI=%d\n",
           SD_LOG_SCK_PIN, SD_LOG_MISO_PIN, SD_LOG_MOSI_PIN);
  SPI.begin(SD_LOG_SCK_PIN, SD_LOG_MISO_PIN, SD_LOG_MOSI_PIN, SD_LOG_CS_PIN);
#endif

  if (!SD.begin(SD_LOG_CS_PIN, SPI, SD_LOG_SPI_FREQ_HZ)) {
    writeLog("[SD] Card init failed; JSONL logging disabled\n");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    writeLog("[SD] No card detected; JSONL logging disabled\n");
    return false;
  }

  if (!openSdLogFile()) {
    return false;
  }

  sdLoggerReady = true;
  writeLog("[SD] BMS readings will be appended to %s\n", SD_LOG_FILE_PATH);
  return true;
}

bool bmsSdLoggerHandleMessage(const BmsMessage &msg)
{
  if (!bmsSdLoggerBegin()) {
    return false;
  }

  if (!openSdLogFile()) {
    sdLoggerReady = false;
    return false;
  }

  size_t written = bmsTelemetryWriteJsonLine(sdLogFile, msg, ++sdLogSequence, millis());
  sdLogFile.flush();

  if (written == 0) {
    writeLog("[SD] Failed to write BMS JSONL record\n");
    return false;
  }

  return true;
}

bool bmsSdLoggerIsReady()
{
  return sdLoggerReady;
}

#else

bool bmsSdLoggerBegin()
{
  return false;
}

bool bmsSdLoggerHandleMessage(const BmsMessage &msg)
{
  (void)msg;
  return false;
}

bool bmsSdLoggerIsReady()
{
  return false;
}

#endif
