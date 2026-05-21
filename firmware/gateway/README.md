# Firmware Gateway

Lê os valores do BMS Felicity/Felicity ESS via RS485 Modbus e transmite os
dados principais da bateria por ESP-NOW.

Este firmware intencionalmente não publica por MQTT e não conecta em uma rede
WiFi. O ESP-NOW usa o rádio do ESP32 diretamente, sem SSID, senha, roteador ou
broker MQTT.

Compilar o gateway ESP32-C3:

```sh
pio run -e esp32-c3-gateway
```

Fazer upload do gateway ESP32-C3:

```sh
pio run -e esp32-c3-gateway -t upload
```

Se mais de uma placa estiver conectada, liste as portas e escolha uma
explicitamente:

```sh
pio device list
pio run -e esp32-c3-gateway -t upload --upload-port COM5
```

Abrir o monitor serial:

```sh
pio device monitor -b 9600
```

Compilar o teste unitário do pacote:

```sh
pio test -e espnow-packet-test --without-uploading --without-testing
```

Rodar o teste unitário em um ESP32-C3 conectado:

```sh
pio test -e espnow-packet-test
```

O pacote binário ESP-NOW compartilhado com o receptor fica em
`../../shared/espnow_battery_packet.h`.
