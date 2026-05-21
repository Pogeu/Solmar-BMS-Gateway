# Gateway Firmware

Reads Felicity/Felicity ESS BMS values over RS485 Modbus and broadcasts the
main battery data over ESP-NOW. In the `esp32dev` environment it also keeps the
existing WiFi, web configuration and MQTT publishing behavior.

Build the default ESP32-C3 serial-only gateway:

```sh
pio run -e esp32-c3-serial
```

Upload the default ESP32-C3 serial-only gateway:

```sh
pio run -e esp32-c3-serial -t upload
```

Build the WiFi/MQTT ESP32 gateway:

```sh
pio run -e esp32dev
```

Upload the WiFi/MQTT ESP32 gateway:

```sh
pio run -e esp32dev -t upload
```

If more than one board is connected, list ports and choose one explicitly:

```sh
pio device list
pio run -e esp32-c3-serial -t upload --upload-port COM5
```

Open the monitor:

```sh
pio device monitor -b 9600
```

Compile the packet unit test:

```sh
pio test -e espnow-packet-test --without-uploading --without-testing
```

Run the packet unit test on a connected ESP32-C3:

```sh
pio test -e espnow-packet-test
```

The binary ESP-NOW packet shared with the receiver lives in
`../../shared/espnow_battery_packet.h`.
