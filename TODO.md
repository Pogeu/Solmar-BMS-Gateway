# TODO

Project task list for the Solmar BMS Gateway firmwares and dashboard.

### Todo

- [ ] Add optional LoRa telemetry [#4](https://github.com/Pogeu/Solmar-BMS-Gateway/issues/4) ~3d #feat #lora
  - [ ] Choose target LoRa module and pinout
  - [ ] Define payload format or reuse the gateway JSON telemetry schema
  - [ ] Add receiver example for LoRa data

- [ ] Add stale-data indicator on the LCD receiver [#5](https://github.com/Pogeu/Solmar-BMS-Gateway/issues/5) ~4h #ux
  - [ ] Show when the last ESP-NOW packet is older than the timeout

- [ ] Add I2C scanner helper for LCD setup [#6](https://github.com/Pogeu/Solmar-BMS-Gateway/issues/6) ~2h #tooling
  - [ ] Print detected LCD backpack addresses on serial monitor

- [ ] Add optional sender MAC filtering [#7](https://github.com/Pogeu/Solmar-BMS-Gateway/issues/7) ~4h #feat #espnow
  - [ ] Accept packets only from a configured gateway MAC address

### In Progress

- [ ] Validate upload and live ESP-NOW reception on the physical ESP32-S3 boards [#8](https://github.com/Pogeu/Solmar-BMS-Gateway/issues/8) ~2h #test

### Done ✓

- [x] Create organized firmware repository ~1d #refactor
- [x] Split gateway and LCD receiver into separate PlatformIO projects ~4h #refactor
- [x] Share the ESP-NOW battery packet format between sender and receiver ~3h #protocol
- [x] Implement ESP32-S3 gateway with RS485 input and ESP-NOW output ~1d #feat
- [x] Remove active MQTT/WiFi publishing from the gateway firmware ~2h #cleanup
- [x] Implement ESP32-S3 16x2 I2C LCD receiver ~1d #feat
- [x] Add ESP-NOW packet unit test build ~3h #test
- [x] Add WiFiManager/MQTT publishing for the LCD-direct gateway ~2d #feat #mqtt
- [x] Add web dashboard for remote battery monitoring ~1d #feat #dashboard
