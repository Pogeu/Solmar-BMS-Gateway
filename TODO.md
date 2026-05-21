# TODO

Project task list for the Solmar BMS ESP-NOW firmwares.

### Todo

- [ ] Add optional MQTT/WiFi publishing ~2d #feat #mqtt
  - [ ] Define configuration flow without making WiFi mandatory
  - [ ] Publish the same main battery fields sent over ESP-NOW
  - [ ] Keep ESP-NOW-only mode as the default firmware path

- [ ] Add optional LoRa telemetry ~3d #feat #lora
  - [ ] Choose target LoRa module and pinout
  - [ ] Define payload format or reuse the ESP-NOW battery packet
  - [ ] Add receiver example for LoRa data

- [ ] Add stale-data indicator on the LCD receiver ~4h #ux
  - [ ] Show when the last ESP-NOW packet is older than the timeout

- [ ] Add I2C scanner helper for LCD setup ~2h #tooling
  - [ ] Print detected LCD backpack addresses on serial monitor

- [ ] Add optional sender MAC filtering ~4h #feat #espnow
  - [ ] Accept packets only from a configured gateway MAC address

### In Progress

- [ ] Validate upload and live ESP-NOW reception on the physical ESP32-C3 boards ~2h #test

### Done ✓

- [x] Create organized firmware repository ~1d #refactor
- [x] Split gateway and LCD receiver into separate PlatformIO projects ~4h #refactor
- [x] Share the ESP-NOW battery packet format between sender and receiver ~3h #protocol
- [x] Implement ESP32-C3 gateway with RS485 input and ESP-NOW output ~1d #feat
- [x] Remove active MQTT/WiFi publishing from the gateway firmware ~2h #cleanup
- [x] Implement ESP32-C3 16x2 I2C LCD receiver ~1d #feat
- [x] Add ESP-NOW packet unit test build ~3h #test
