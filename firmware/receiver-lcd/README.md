# Display local via ESP-NOW

Firmware para usar o display local em um segundo ESP32-S3. Ele recebe o broadcast de
bateria enviado pelo gateway via ESP-NOW e mostra os principais valores em um
display grafico 128x64 SPI para quem esta perto do barco.

Esta é uma das topologias possíveis do projeto. Quando o display fica conectado na
mesma placa que lê o RS485, use o ambiente `esp32-s3-gateway-lcd-direct`.

Ligação padrão:

- Display VDD -> 3V3 ou 5V, conforme o módulo
- Display VSS -> GND
- Display SCL -> GPIO18
- Display SI -> GPIO23
- Display CS -> GPIO15
- Display RS -> GPIO16
- Display RSE -> GPIO17
- Backlight A -> VCC
- Backlight K -> GND
- Botão de página -> GPIO4 e GND

Configurações padrão em `platformio.ini`:

- Canal ESP-NOW: `1`
- Display: `ST7565` 128x64 SPI
- Botão de troca de página: `GPIO4` com pull-up interno

Leitura do display:

- Página 1: SOC e potência com destaque e barra de progresso.
- Página 2: tensão, corrente, potência e SOC.
- Página 3: comparação entre SOC do BMS e SOC estimado pela tabela LiFePO4
  48V da Jackery. Essa estimativa por tensão só é confiável com bateria em
  repouso.
- Página 4: temperatura e estado de carga/descarga.
- Página 5: falhas e idade da última leitura.
- Se nenhum pacote novo chegar por mais de 10 segundos, a linha 2 mostra
  `Link OLD` na página 5, e o cabeçalho mostra o status antigo.
- O botão de página deve ligar `GPIO4` ao `GND`; o firmware usa `INPUT_PULLUP`,
  interrupção por mudança de estado e debounce não bloqueante no estilo da
  `EasyButtonAtInt01`.
- Os pinos `IC_SCL`, `IC_CS`, `IC_SO` e `IC_SI` da placa do display não são
  usados na comunicação principal.

Se o display acender mas não mostrar texto, revise primeiro a pinagem `SCL`,
`SI`, `CS`, `RS` e `RSE` e, se necessário, teste outra variante ST7565 da
biblioteca U8g2. O canal ESP-NOW precisa ser igual ao `ESP_NOW_WIFI_CHANNEL` do firmware
transmissor. O formato binário do pacote é compartilhado por
`../../shared/espnow_battery_packet.h`.

Compilar e fazer upload:

```sh
pio run -e esp32-s3-lcd-receiver
pio run -e esp32-s3-lcd-receiver -t upload
pio device monitor -b 115200
```

Teste local das telas do LCD, sem depender do gateway ESP-NOW:

```sh
pio run -e esp32-s3-lcd-pages-test
pio run -e esp32-s3-lcd-pages-test -t upload
pio device monitor -b 115200
```

Esse modo gera leituras falsas a cada segundo para testar a tela principal,
as páginas secundárias e o botão em `GPIO4`.

Se mais de um ESP32 estiver conectado, liste as portas primeiro:

```sh
pio device list
```

Depois faça upload explicitamente para a porta do receptor:

```sh
pio run -e esp32-s3-lcd-receiver -t upload --upload-port COM6
```

Se o display continuar mostrando `Sem dados`, confirme que transmissor e
receptor usam o mesmo `ESP_NOW_WIFI_CHANNEL`.
