# Solmar BMS ESP-NOW

Monorepo for the Solmar battery telemetry firmware.

It contains two independent PlatformIO firmwares plus one shared protocol
header:

- `firmware/gateway`: reads Felicity/Felicity ESS BMS data over RS485 Modbus,
  publishes to MQTT in WiFi mode, and broadcasts the main battery data over
  ESP-NOW.
- `firmware/receiver-lcd`: receives the ESP-NOW battery packet on a second
  ESP32-C3 and shows the main values on a 16x2 I2C LCD.
- `shared`: common ESP-NOW packet format used by both firmwares.

Keeping the ESP-NOW packet in `shared/espnow_battery_packet.h` avoids a common
embedded bug: the sender and receiver silently drifting to different binary
layouts.

## Gateway

Build the serial-only ESP32-C3 gateway:

```sh
cd firmware/gateway
pio run -e esp32-c3-serial
```

Build the ESP32 WiFi/MQTT gateway:

```sh
cd firmware/gateway
pio run -e esp32dev
```

Compile the ESP-NOW packet unit test:

```sh
cd firmware/gateway
pio test -e espnow-packet-test --without-uploading --without-testing
```

## LCD Receiver

Build the ESP32-C3 LCD receiver:

```sh
cd firmware/receiver-lcd
pio run -e esp32-c3-lcd-receiver
```

Upload with:

```sh
pio run -e esp32-c3-lcd-receiver -t upload
pio device monitor -b 115200
```

## ESP-NOW Channel

The receiver and sender must use the same ESP-NOW channel.

For the serial-only gateway, this is `ESP_NOW_WIFI_CHANNEL` in
`firmware/gateway/platformio.ini`.

For the WiFi/MQTT gateway, the radio follows the connected access point channel,
so set the receiver's `ESP_NOW_WIFI_CHANNEL` to the WiFi router channel.
