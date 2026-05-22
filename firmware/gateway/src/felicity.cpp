//
// This is part of the FelicityBMS2MQTT project
//
// https://github.com/Smartsmurf/FelicityBMS2MQTT
//
//
#include <Arduino.h>
#include <string.h>

#include "crc.h"
#include "felicity.h"
#include "main.h"

volatile bool systemShutdown = false;

static const uint16_t REG_VERSION = 0xF80B;
static const uint16_t LEN_VERSION = 0x0001;

static const uint16_t REG_BATTERY_INFO = 0x1302;
static const uint16_t LEN_BATTERY_INFO = 0x000A;

static const uint16_t REG_LIMITS = 0x131C;
static const uint16_t LEN_LIMITS = 0x0004;

static const uint16_t REG_CELLS_TEMPS = 0x132A;
static const uint16_t LEN_CELLS_TEMPS_FULL = 0x0018;
static const uint16_t LEN_CELLS_TEMPS_SHORT = 0x0014;

FelicityBMS::FelicityBMS(int rx, int tx, int num_slaves)
    : FelicityBMS(rx, tx, RS485_DE_PIN, RS485_RE_PIN, num_slaves)
{
}

FelicityBMS::FelicityBMS(int rx, int tx, int de, int re, int num_slaves)
{
    this->dePin = de;
    this->rePin = re;

    pinMode(this->dePin, OUTPUT);
    pinMode(this->rePin, OUTPUT);
    enableRx();

    this->serial = new HardwareSerial(1);
    this->serial->setRxBufferSize(256);
    this->serial->begin(BMS_BAUD_RATE, SERIAL_8N1, rx, tx);
    this->num_slaves = num_slaves;
}

FelicityBMS::~FelicityBMS()
{
    delete serial;
}

void FelicityBMS::enableTx()
{
    digitalWrite(this->dePin, HIGH);
    digitalWrite(this->rePin, HIGH);
    delayMicroseconds(800);
}

void FelicityBMS::enableRx()
{
    digitalWrite(this->dePin, LOW);
    digitalWrite(this->rePin, LOW);
    delayMicroseconds(800);
}

void FelicityBMS::clearInput()
{
    while (this->serial->available()) {
        this->serial->read();
    }
}

static bool isInvalidRaw(uint16_t raw)
{
    return raw == 0x7FFF || raw == 0xFFFF;
}

static bool isValidCellRaw(uint16_t raw)
{
    if (isInvalidRaw(raw) || raw == 0x0000) {
        return false;
    }

    float voltage = raw * 0.001f;
    return voltage >= 2.0f && voltage <= 4.0f;
}

static bool decodeTemperatureRaw(uint16_t raw, float *tempC)
{
    if (tempC == nullptr || isInvalidRaw(raw)) {
        return false;
    }

    int16_t signedRaw = (int16_t)raw;

    if (signedRaw >= -40 && signedRaw <= 125) {
        *tempC = (float)signedRaw;
        return true;
    }

    float tempDiv10 = signedRaw / 10.0f;
    if (tempDiv10 >= -40.0f && tempDiv10 <= 125.0f) {
        *tempC = tempDiv10;
        return true;
    }

    return false;
}

static void logModbusFrame(const char *label, const uint8_t *buffer, size_t len)
{
#if FELICITY_VERBOSE_MODBUS
    writeLog("%s [%u bytes]: ", label, (unsigned)len);
    for (size_t i = 0; i < len; i++) {
        writeLog("%02X ", buffer[i]);
    }
    writeLog("\n");
#else
    (void)label;
    (void)buffer;
    (void)len;
#endif
}

uint16_t FelicityBMS::SendAPDU(uint8_t sid, uint8_t cmd, uint16_t addr, uint16_t len)
{
    uint8_t buffer[8];
    ModbusCRC crc;

    this->lastSid = sid;
    this->lastCmd = cmd;

    buffer[0] = sid;
    buffer[1] = cmd;
    buffer[2] = (addr >> 8);
    buffer[3] = addr & 0xff;
    buffer[4] = (len >> 8);
    buffer[5] = len & 0xff;

    uint16_t csum = crc.crc16_modbus(buffer, 6);
    buffer[6] = csum & 0xff;
    buffer[7] = (csum >> 8);

    clearInput();
    logModbusFrame("TX", buffer, sizeof(buffer));

    enableTx();
    this->serial->write(buffer, sizeof(buffer));
    this->serial->flush();
    delayMicroseconds(1800);
    enableRx();

    return 1;
}

int FelicityBMS::ReceiveAPDU(uint8_t *buffer, uint16_t len)
{
    uint8_t response[160];
    size_t n = 0;
    uint32_t start = millis();

    while (millis() - start < 900) {
        while (this->serial->available()) {
            if (n < sizeof(response)) {
                response[n++] = (uint8_t)this->serial->read();
            } else {
                this->serial->read();
            }
        }

        if (n >= 5 && (response[1] & 0x80)) {
            break;
        }

        if (n >= 5) {
            uint8_t byteCount = response[2];
            size_t expected = 3 + byteCount + 2;
            if (expected <= sizeof(response) && n >= expected) {
                break;
            }
        }

        delay(1);
    }

    if (n == 0) {
#if FELICITY_VERBOSE_MODBUS
        writeLog("RX: no response\n");
#endif
        return -1;
    }

    logModbusFrame("RX", response, n);

    if (n < 5) {
        return -2;
    }

    if (response[1] & 0x80) {
#if FELICITY_VERBOSE_MODBUS
        writeLog("Modbus exception: 0x%02X\n", response[2]);
#endif
        return -5;
    }

    if (response[0] != this->lastSid) {
        return -4;
    }

    uint8_t byteCount = response[2];
    size_t expected = 3 + byteCount + 2;

    if (expected > sizeof(response)) {
        return -9;
    }

    if (n < expected) {
        return -8;
    }

    uint16_t crcRx = ((uint16_t)response[expected - 1] << 8) | response[expected - 2];
    ModbusCRC crc;
    uint16_t crcCalc = crc.crc16_modbus(response, expected - 2);

    if (crcRx != crcCalc) {
#if FELICITY_VERBOSE_MODBUS
        writeLog("Invalid CRC rx=0x%04X calc=0x%04X\n", crcRx, crcCalc);
#endif
        return -3;
    }

    if (response[1] != this->lastCmd) {
        return -6;
    }

    if (byteCount > len) {
        return -7;
    }

    memcpy(buffer, &response[3], byteCount);
    return byteCount;
}

int FelicityBMS::readHolding(uint8_t sid, uint16_t addr, uint16_t count, uint8_t *payload, size_t payloadMax)
{
#if FELICITY_VERBOSE_MODBUS
    writeLog("\nSlave 0x%02X Reg 0x%04X Len %u\n", sid, addr, (unsigned)count);
#endif

    SendAPDU(sid, 0x03, addr, count);
    int rlen = ReceiveAPDU(payload, payloadMax);
    if (rlen < 0) {
#if FELICITY_VERBOSE_MODBUS
        writeLog("Read failed: %d\n", rlen);
#endif
        return rlen;
    }

    return rlen;
}

void FelicityBMS::SetQueue(QueueHandle_t bmsq)
{
    bmsQueue = bmsq;
}

static void sendVersionMessage(QueueHandle_t queue, uint8_t sid, const uint8_t *buffer)
{
    BmsMessage msg = {};
    msg.deviceId = sid;
    msg.type = BMS_TYPE_VERSION_INFO;
    msg.payload.versionInfo.version = be16(buffer);
    xQueueSend(queue, &msg, portMAX_DELAY);
}

static void sendBatteryInfoMessage(QueueHandle_t queue, uint8_t sid, const uint8_t *buffer)
{
    BmsMessage msg = {};
    msg.deviceId = sid;
    msg.type = BMS_TYPE_BATTERY_INFO;

    msg.payload.batteryInfo.batteryChargeEnable = (buffer[1]) & 1;
    msg.payload.batteryInfo.batteryChargeImmediately = (buffer[1] >> 1) & 1;
    msg.payload.batteryInfo.batteryDischargeEnable = (buffer[1] >> 2) & 1;

    msg.payload.batteryInfo.faultCellTemperatureHigh = (buffer[4]) & 1;
    msg.payload.batteryInfo.faultCellTemperatureLow = (buffer[4] >> 1) & 1;
    msg.payload.batteryInfo.faultCellVoltageHigh = (buffer[5] >> 2) & 1;
    msg.payload.batteryInfo.faultCellVoltageLow = (buffer[5] >> 3) & 1;
    msg.payload.batteryInfo.faultChargeCurrentHigh = (buffer[5] >> 4) & 1;
    msg.payload.batteryInfo.faultDischargeCurrentHigh = (buffer[5] >> 5) & 1;
    msg.payload.batteryInfo.faultBMSTemperatureHigh = (buffer[5] >> 6) & 1;

    msg.payload.batteryInfo.voltage = be16(&buffer[8]) * 0.01f;
    msg.payload.batteryInfo.current = (int16_t)be16(&buffer[10]) * 0.1f;
    msg.payload.batteryInfo.packPowerW =
        msg.payload.batteryInfo.voltage * msg.payload.batteryInfo.current;
    msg.payload.batteryInfo.soc = be16(&buffer[18]);

    float tempC = 0.0f;
    if (decodeTemperatureRaw(be16(&buffer[16]), &tempC)) {
        msg.payload.batteryInfo.batteryTemperatureValid = true;
        msg.payload.batteryInfo.tempC = tempC;
    }

    xQueueSend(queue, &msg, portMAX_DELAY);
}

static void sendLimitsMessage(QueueHandle_t queue, uint8_t sid, const uint8_t *buffer)
{
    BmsMessage msg = {};
    msg.deviceId = sid;
    msg.type = BMS_TYPE_CHARGE_DISCHARGE;
    msg.payload.chargeDischarge.chargeVoltLimit = be16(&buffer[0]) * 0.01f;
    msg.payload.chargeDischarge.dischargeVoltLimit = be16(&buffer[2]) * 0.01f;
    msg.payload.chargeDischarge.chargeCurrentLimit = be16(&buffer[4]) * 0.1f;
    msg.payload.chargeDischarge.dischargeCurrentLimit = be16(&buffer[6]) * 0.1f;
    xQueueSend(queue, &msg, portMAX_DELAY);
}

static bool fillCellsTempsMessage(BmsMessage *msg, const uint8_t *buffer, uint16_t regsRead)
{
    if (msg == nullptr || regsRead < 20) {
        return false;
    }

    msg->type = BMS_TYPE_CELL_VOLTAGES;
    msg->payload.cellInfo.cellsTempsRegsRead = regsRead;

    for (int j = 0; j < FELICITY_MAX_CELL_SLOTS; j++) {
        uint16_t raw = be16(&buffer[j * 2]);

        if (!isValidCellRaw(raw)) {
            continue;
        }

        uint8_t idx = msg->payload.cellInfo.validCellCount++;
        float voltage = raw * 0.001f;
        msg->payload.cellInfo.cellVoltages[idx] = voltage;
        msg->payload.cellInfo.cellSumV += voltage;

        if (idx == 0) {
            msg->payload.cellInfo.cellMinV = voltage;
            msg->payload.cellInfo.cellMaxV = voltage;
        } else {
            msg->payload.cellInfo.cellMinV = min(msg->payload.cellInfo.cellMinV, voltage);
            msg->payload.cellInfo.cellMaxV = max(msg->payload.cellInfo.cellMaxV, voltage);
        }
    }

    if (msg->payload.cellInfo.validCellCount > 0) {
        msg->payload.cellInfo.cellAvgV =
            msg->payload.cellInfo.cellSumV / msg->payload.cellInfo.validCellCount;
        msg->payload.cellInfo.cellDeltaMv =
            (msg->payload.cellInfo.cellMaxV - msg->payload.cellInfo.cellMinV) * 1000.0f;
    }

    uint16_t tempSlots = regsRead - FELICITY_MAX_CELL_SLOTS;
    if (tempSlots > FELICITY_MAX_TEMPERATURE_SLOTS) {
        tempSlots = FELICITY_MAX_TEMPERATURE_SLOTS;
    }

    float tempSum = 0.0f;
    for (uint16_t j = 0; j < tempSlots; j++) {
        uint16_t raw = be16(&buffer[(FELICITY_MAX_CELL_SLOTS + j) * 2]);
        float tempC = 0.0f;

        if (!decodeTemperatureRaw(raw, &tempC)) {
            continue;
        }

        uint8_t idx = msg->payload.cellInfo.validTemperatureCount++;
        msg->payload.cellInfo.cellTemperatures[idx] = tempC;
        tempSum += tempC;

        if (idx == 0) {
            msg->payload.cellInfo.tempMinC = tempC;
            msg->payload.cellInfo.tempMaxC = tempC;
        } else {
            msg->payload.cellInfo.tempMinC = min(msg->payload.cellInfo.tempMinC, tempC);
            msg->payload.cellInfo.tempMaxC = max(msg->payload.cellInfo.tempMaxC, tempC);
        }
    }

    if (msg->payload.cellInfo.validTemperatureCount > 0) {
        msg->payload.cellInfo.tempAvgC =
            tempSum / msg->payload.cellInfo.validTemperatureCount;
        msg->payload.cellInfo.tempDeltaC =
            msg->payload.cellInfo.tempMaxC - msg->payload.cellInfo.tempMinC;
    }

    return msg->payload.cellInfo.validCellCount > 0 ||
           msg->payload.cellInfo.validTemperatureCount > 0;
}

void FelicityBMS::bmsTask(void *param)
{
    (void)param;

    uint8_t buffer[64];

    for (;;) {
        if (systemShutdown) {
            writeLog("[BMS] Shutting down task...\n");
            vTaskDelete(NULL);
        }

        for (int i = 0; i < num_slaves; i++) {
            uint8_t sid = (uint8_t)i + 1;

            int rlen = readHolding(sid, REG_VERSION, LEN_VERSION, buffer, sizeof(buffer));
            if (rlen == (int)(LEN_VERSION * 2)) {
                sendVersionMessage(bmsQueue, sid, buffer);
            }

            vTaskDelay(120 / portTICK_PERIOD_MS);

            rlen = readHolding(sid, REG_BATTERY_INFO, LEN_BATTERY_INFO, buffer, sizeof(buffer));
            if (rlen == (int)(LEN_BATTERY_INFO * 2)) {
                sendBatteryInfoMessage(bmsQueue, sid, buffer);
            }

            vTaskDelay(120 / portTICK_PERIOD_MS);

            rlen = readHolding(sid, REG_LIMITS, LEN_LIMITS, buffer, sizeof(buffer));
            if (rlen == (int)(LEN_LIMITS * 2)) {
                sendLimitsMessage(bmsQueue, sid, buffer);
            }

            vTaskDelay(120 / portTICK_PERIOD_MS);

            BmsMessage msg = {};
            msg.deviceId = sid;

            rlen = readHolding(sid, REG_CELLS_TEMPS, LEN_CELLS_TEMPS_FULL, buffer, sizeof(buffer));
            if (rlen == (int)(LEN_CELLS_TEMPS_FULL * 2)) {
                if (fillCellsTempsMessage(&msg, buffer, LEN_CELLS_TEMPS_FULL)) {
                    xQueueSend(bmsQueue, &msg, portMAX_DELAY);
                }
            } else {
                rlen = readHolding(sid, REG_CELLS_TEMPS, LEN_CELLS_TEMPS_SHORT, buffer, sizeof(buffer));
                if (rlen == (int)(LEN_CELLS_TEMPS_SHORT * 2)) {
                    if (fillCellsTempsMessage(&msg, buffer, LEN_CELLS_TEMPS_SHORT)) {
                        xQueueSend(bmsQueue, &msg, portMAX_DELAY);
                    }
                }
            }

            vTaskDelay(300 / portTICK_PERIOD_MS);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
