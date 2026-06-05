#ifndef SOLMAR_BMS_TELEMETRY_JSON_H
#define SOLMAR_BMS_TELEMETRY_JSON_H

#include <Arduino.h>
#include <string.h>

#include "felicity.h"

#define BMS_TELEMETRY_JSON_SCHEMA "solmar.bms.reading.v1"

class BmsTelemetryBufferPrint : public Print {
public:
  BmsTelemetryBufferPrint(char *buffer, size_t bufferSize)
      : buffer_(buffer), bufferSize_(buffer == nullptr ? 0 : bufferSize)
  {
    if (buffer_ != nullptr && bufferSize_ > 0) {
      buffer_[0] = '\0';
    }
  }

  size_t write(uint8_t value) override
  {
    return write(&value, 1);
  }

  size_t write(const uint8_t *data, size_t size) override
  {
    if (data == nullptr || size == 0) {
      return 0;
    }

    size_t copied = 0;
    if (buffer_ != nullptr && bufferSize_ > 0 && length_ < bufferSize_ - 1) {
      size_t available = (bufferSize_ - 1) - length_;
      copied = size < available ? size : available;
      memcpy(buffer_ + length_, data, copied);
    }

    length_ += size;
    if (buffer_ != nullptr && bufferSize_ > 0) {
      size_t terminator = length_ < bufferSize_ ? length_ : bufferSize_ - 1;
      buffer_[terminator] = '\0';
    }

    if (copied < size) {
      truncated_ = true;
    }

    return size;
  }

  bool truncated() const
  {
    return truncated_;
  }

  size_t length() const
  {
    return length_;
  }

private:
  char *buffer_;
  size_t bufferSize_;
  size_t length_ = 0;
  bool truncated_ = false;
};

inline const char *bmsTelemetryMessageTypeName(BmsDataType type)
{
  switch (type) {
    case BMS_TYPE_VERSION_INFO:
      return "version_info";

    case BMS_TYPE_BATTERY_INFO:
      return "battery_info";

    case BMS_TYPE_CHARGE_DISCHARGE:
      return "charge_discharge";

    case BMS_TYPE_CELL_VOLTAGES:
      return "cell_voltages";

    default:
      return "unknown";
  }
}

inline size_t bmsTelemetryWriteBool(Print &out, bool value)
{
  return out.print(value ? "true" : "false");
}

inline size_t bmsTelemetryWriteNullableFloat(Print &out, bool valid, float value, uint8_t digits)
{
  if (!valid) {
    return out.print("null");
  }

  return out.print(value, digits);
}

inline size_t bmsTelemetryWriteFloatArray(Print &out, const float *values, uint8_t count, uint8_t maxCount,
                                          uint8_t digits)
{
  size_t written = out.print('[');
  uint8_t cappedCount = count < maxCount ? count : maxCount;

  for (uint8_t i = 0; i < cappedCount; i++) {
    if (i > 0) {
      written += out.print(',');
    }
    written += out.print(values[i], digits);
  }

  written += out.print(']');
  return written;
}

inline size_t bmsTelemetryWriteVersionData(Print &out, const BmsMessage &msg)
{
  size_t written = 0;
  written += out.print("\"version\":");
  written += out.print((unsigned)msg.payload.versionInfo.version);
  return written;
}

inline size_t bmsTelemetryWriteBatteryData(Print &out, const BmsMessage &msg)
{
  const auto &battery = msg.payload.batteryInfo;
  size_t written = 0;

  written += out.print("\"voltage_v\":");
  written += out.print(battery.voltage, 2);
  written += out.print(",\"current_a\":");
  written += out.print(battery.current, 2);
  written += out.print(",\"pack_power_w\":");
  written += out.print(battery.packPowerW, 1);
  written += out.print(",\"soc_percent\":");
  written += out.print((unsigned)battery.soc);
  written += out.print(",\"temperature_c\":");
  written += bmsTelemetryWriteNullableFloat(out, battery.batteryTemperatureValid, battery.tempC, 1);
  written += out.print(",\"battery_temperature_valid\":");
  written += bmsTelemetryWriteBool(out, battery.batteryTemperatureValid);
  written += out.print(",\"battery_charge_enable\":");
  written += bmsTelemetryWriteBool(out, battery.batteryChargeEnable);
  written += out.print(",\"battery_charge_immediately\":");
  written += bmsTelemetryWriteBool(out, battery.batteryChargeImmediately);
  written += out.print(",\"battery_discharge_enable\":");
  written += bmsTelemetryWriteBool(out, battery.batteryDischargeEnable);
  written += out.print(",\"fault_cell_voltage_high\":");
  written += bmsTelemetryWriteBool(out, battery.faultCellVoltageHigh);
  written += out.print(",\"fault_cell_voltage_low\":");
  written += bmsTelemetryWriteBool(out, battery.faultCellVoltageLow);
  written += out.print(",\"fault_charge_current_high\":");
  written += bmsTelemetryWriteBool(out, battery.faultChargeCurrentHigh);
  written += out.print(",\"fault_discharge_current_high\":");
  written += bmsTelemetryWriteBool(out, battery.faultDischargeCurrentHigh);
  written += out.print(",\"fault_bms_temperature_high\":");
  written += bmsTelemetryWriteBool(out, battery.faultBMSTemperatureHigh);
  written += out.print(",\"fault_cell_temperature_high\":");
  written += bmsTelemetryWriteBool(out, battery.faultCellTemperatureHigh);
  written += out.print(",\"fault_cell_temperature_low\":");
  written += bmsTelemetryWriteBool(out, battery.faultCellTemperatureLow);

  return written;
}

inline size_t bmsTelemetryWriteLimitsData(Print &out, const BmsMessage &msg)
{
  const auto &limits = msg.payload.chargeDischarge;
  size_t written = 0;

  written += out.print("\"charge_voltage_limit_v\":");
  written += out.print(limits.chargeVoltLimit, 2);
  written += out.print(",\"discharge_voltage_limit_v\":");
  written += out.print(limits.dischargeVoltLimit, 2);
  written += out.print(",\"charge_current_limit_a\":");
  written += out.print(limits.chargeCurrentLimit, 1);
  written += out.print(",\"discharge_current_limit_a\":");
  written += out.print(limits.dischargeCurrentLimit, 1);

  return written;
}

inline size_t bmsTelemetryWriteCellsData(Print &out, const BmsMessage &msg)
{
  const auto &cells = msg.payload.cellInfo;
  size_t written = 0;

  written += out.print("\"cells_temps_regs_read\":");
  written += out.print((unsigned)cells.cellsTempsRegsRead);
  written += out.print(",\"valid_cell_count\":");
  written += out.print((unsigned)cells.validCellCount);
  written += out.print(",\"cell_voltages_v\":");
  written += bmsTelemetryWriteFloatArray(out, cells.cellVoltages, cells.validCellCount,
                                         FELICITY_MAX_CELL_SLOTS, 3);
  written += out.print(",\"cell_min_v\":");
  written += out.print(cells.cellMinV, 3);
  written += out.print(",\"cell_max_v\":");
  written += out.print(cells.cellMaxV, 3);
  written += out.print(",\"cell_avg_v\":");
  written += out.print(cells.cellAvgV, 3);
  written += out.print(",\"cell_sum_v\":");
  written += out.print(cells.cellSumV, 3);
  written += out.print(",\"cell_delta_mv\":");
  written += out.print(cells.cellDeltaMv, 0);
  written += out.print(",\"valid_temperature_count\":");
  written += out.print((unsigned)cells.validTemperatureCount);
  written += out.print(",\"cell_temperatures_c\":");
  written += bmsTelemetryWriteFloatArray(out, cells.cellTemperatures, cells.validTemperatureCount,
                                         FELICITY_MAX_TEMPERATURE_SLOTS, 1);
  written += out.print(",\"temp_min_c\":");
  written += out.print(cells.tempMinC, 1);
  written += out.print(",\"temp_max_c\":");
  written += out.print(cells.tempMaxC, 1);
  written += out.print(",\"temp_avg_c\":");
  written += out.print(cells.tempAvgC, 1);
  written += out.print(",\"temp_delta_c\":");
  written += out.print(cells.tempDeltaC, 1);

  return written;
}

inline size_t bmsTelemetryWriteData(Print &out, const BmsMessage &msg)
{
  switch (msg.type) {
    case BMS_TYPE_VERSION_INFO:
      return bmsTelemetryWriteVersionData(out, msg);

    case BMS_TYPE_BATTERY_INFO:
      return bmsTelemetryWriteBatteryData(out, msg);

    case BMS_TYPE_CHARGE_DISCHARGE:
      return bmsTelemetryWriteLimitsData(out, msg);

    case BMS_TYPE_CELL_VOLTAGES:
      return bmsTelemetryWriteCellsData(out, msg);

    default:
      return out.print("\"unknown\":true");
  }
}

inline size_t bmsTelemetryWriteJson(Print &out, const BmsMessage &msg, uint32_t sequence, uint32_t uptimeMs)
{
  size_t written = 0;

  written += out.print('{');
  written += out.print("\"schema\":\"" BMS_TELEMETRY_JSON_SCHEMA "\"");
  written += out.print(",\"sequence\":");
  written += out.print((unsigned long)sequence);
  written += out.print(",\"uptime_ms\":");
  written += out.print((unsigned long)uptimeMs);
  written += out.print(",\"device_id\":");
  written += out.print((unsigned)msg.deviceId);
  written += out.print(",\"type\":\"");
  written += out.print(bmsTelemetryMessageTypeName(msg.type));
  written += out.print("\",\"data\":{");
  written += bmsTelemetryWriteData(out, msg);
  written += out.print("}}");

  return written;
}

inline size_t bmsTelemetryWriteJsonLine(Print &out, const BmsMessage &msg, uint32_t sequence, uint32_t uptimeMs)
{
  size_t written = bmsTelemetryWriteJson(out, msg, sequence, uptimeMs);
  written += out.print('\n');
  return written;
}

inline bool bmsTelemetryBuildJson(const BmsMessage &msg, uint32_t sequence, uint32_t uptimeMs,
                                  char *buffer, size_t bufferSize)
{
  BmsTelemetryBufferPrint out(buffer, bufferSize);
  bmsTelemetryWriteJson(out, msg, sequence, uptimeMs);
  return buffer != nullptr && bufferSize > 0 && !out.truncated();
}

#endif // SOLMAR_BMS_TELEMETRY_JSON_H
