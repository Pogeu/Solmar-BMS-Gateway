# Receptor ESP-NOW com LCD

Firmware para um segundo ESP32-C3 que recebe o broadcast de bateria enviado pelo
gateway via ESP-NOW e mostra os principais valores em um LCD 16x2 I2C.

Ligação padrão:

- LCD VCC -> 5V ou 3V3, conforme o backpack do LCD
- LCD GND -> GND
- LCD SDA -> GPIO8
- LCD SCL -> GPIO9

Configurações padrão em `platformio.ini`:

- Canal ESP-NOW: `1`
- Endereço I2C do LCD: `0x27`
- Tamanho do LCD: `16x2`

Se o LCD acender mas não mostrar texto, tente trocar `LCD_I2C_ADDR` para
`0x3F`. O canal ESP-NOW precisa ser igual ao `ESP_NOW_WIFI_CHANNEL` do firmware
transmissor. O formato binário do pacote é compartilhado por
`../../shared/espnow_battery_packet.h`.

Compilar e fazer upload:

```sh
pio run -e esp32-c3-lcd-receiver
pio run -e esp32-c3-lcd-receiver -t upload
pio device monitor -b 115200
```

Se mais de um ESP32 estiver conectado, liste as portas primeiro:

```sh
pio device list
```

Depois faça upload explicitamente para a porta do receptor:

```sh
pio run -e esp32-c3-lcd-receiver -t upload --upload-port COM6
```

Se o display continuar mostrando `Sem dados`, confirme que transmissor e
receptor usam o mesmo `ESP_NOW_WIFI_CHANNEL`.
