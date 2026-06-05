#ifndef SOLMAR_BMS_SD_LOGGER_H
#define SOLMAR_BMS_SD_LOGGER_H

#include <Arduino.h>

#include "felicity.h"

#ifndef SD_LOG_ENABLE
#define SD_LOG_ENABLE 0
#endif

#ifndef SD_LOG_FILE_PATH
#define SD_LOG_FILE_PATH "/bms_log.jsonl"
#endif

#ifndef SD_LOG_CS_PIN
#define SD_LOG_CS_PIN 7
#endif

#ifndef SD_LOG_SCK_PIN
#define SD_LOG_SCK_PIN 4
#endif

#ifndef SD_LOG_MISO_PIN
#define SD_LOG_MISO_PIN 5
#endif

#ifndef SD_LOG_MOSI_PIN
#define SD_LOG_MOSI_PIN 6
#endif

#ifndef SD_LOG_USE_DEFAULT_SPI_PINS
#define SD_LOG_USE_DEFAULT_SPI_PINS 1
#endif

#ifndef SD_LOG_SPI_FREQ_HZ
#define SD_LOG_SPI_FREQ_HZ 4000000UL
#endif

bool bmsSdLoggerBegin();
bool bmsSdLoggerHandleMessage(const BmsMessage &msg);
bool bmsSdLoggerIsReady();

#endif // SOLMAR_BMS_SD_LOGGER_H
