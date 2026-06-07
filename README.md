# Solmar BMS Gateway

Repositório dos firmwares e da página de telemetria da bateria do projeto
Solmar. O objetivo do sistema é ser a interface entre a bateria do barco e as
pessoas que precisam acompanhar seu estado: o usuário perto do barco pelo LCD e
a equipe remota pelo dashboard MQTT.

> [!NOTE]  
> O modelo da bateria que esta sendo utilizada é **Felicity FLA12171-EU**.

ESP-NOW, MQTT/WiFi, microSD e futuras conexões GSM são meios de transportar ou
registrar os dados. O foco do projeto é entregar leitura clara, local e remota,
dos dados do BMS.

O repositório contém:

- `firmware/gateway`: lê os dados do BMS Felicity/Felicity ESS via RS485 Modbus
  e atua como origem da telemetria.
- `firmware/receiver-lcd`: mostra os principais valores em um LCD 16x2 I2C para
  quem esta perto do barco. Nesta topologia ele recebe os dados por ESP-NOW.
- `dashboard`: página web para a equipe acompanhar a bateria a distancia por
  MQTT.
- `shared`: formato comum do pacote ESP-NOW usado quando o LCD local fica em um
  segundo ESP32-S3.

## Visão geral

O gateway fica conectado ao barramento RS485 da bateria e transforma as leituras
do BMS em informação de uso:

- LCD local: leitura rápida para operação e diagnóstico perto do barco.
- Dashboard remoto: visão para a equipe quando ela não esta no barco.
- Log microSD: histórico simples em JSON Lines para análise posterior.
- ESP-NOW: transporte local sem roteador quando o LCD esta em outra placa.
- MQTT/WiFi: transporte atual para o dashboard remoto.

O ambiente mais completo hoje é `esp32-s3-gateway-lcd-direct`: uma placa lê a
BMS via RS485, atualiza o LCD local, grava no microSD e publica no MQTT. O
ambiente `esp32-s3-gateway` continua disponível quando for melhor separar a
placa que lê a BMS da placa que mostra o LCD.

Possíveis integrações futuras ficam listadas em [TODO.md](TODO.md), incluindo
LoRa e troca do backend WiFi por GSM.

## Gateway RS485

O gateway é a placa conectada ao barramento RS485 da bateria. Ele pode operar em
duas topologias principais:

- `esp32-s3-gateway`: lê a BMS e envia um pacote ESP-NOW para outro ESP32-S3.
- `esp32-s3-gateway-lcd-direct`: lê a BMS, atualiza o LCD local, grava microSD e
  publica MQTT para o dashboard.

Se o comando `pio` não estiver disponível no terminal, substitua `pio` pelo
caminho completo do PlatformIO:

```sh
C:\Users\pedro\.platformio\penv\Scripts\platformio.exe
```

Antes de fazer upload, liste as portas seriais conectadas:

```sh
pio device list
```

Se apenas um ESP32 estiver conectado, o PlatformIO geralmente detecta a porta
automaticamente. Se houver mais de uma placa conectada, informe a porta
explicitamente com `--upload-port COMx`.

Compilar o gateway ESP32-S3 com saída ESP-NOW sem fazer upload:

```sh
cd firmware/gateway
pio run -e esp32-s3-gateway
```

Fazer upload do gateway ESP32-S3:

```sh
cd firmware/gateway
pio run -e esp32-s3-gateway -t upload
```

Fazer upload em uma porta específica:

```sh
pio run -e esp32-s3-gateway -t upload --upload-port COM5
```

Abrir o monitor serial do gateway:

```sh
pio device monitor -b 9600
```

Compilar o teste unitário do pacote ESP-NOW:

```sh
cd firmware/gateway
pio test -e espnow-packet-test --without-uploading --without-testing
```

## Verificações no GitHub

O repositório tem um workflow para pull requests:

- `.github/workflows/ci.yml`: compila o firmware do gateway, compila os testes
  PlatformIO e compila o receptor LCD.

## LCD local

O LCD local é a interface para quem esta perto do barco. Ele mostra SOC,
potência, tensão, corrente, temperatura, status de carga/descarga, falhas e
idade da última leitura.

Existem duas formas de usar o LCD:

- LCD direto no gateway: usado pelo ambiente `esp32-s3-gateway-lcd-direct`.
- LCD em uma segunda placa: usado pelo `firmware/receiver-lcd`, recebendo dados
  por ESP-NOW.

Desconecte a placa do gateway ou use `--upload-port COMx` para evitar gravar o
firmware do receptor na placa errada.

Compilar o receptor LCD ESP32-S3 sem fazer upload:

```sh
cd firmware/receiver-lcd
pio run -e esp32-s3-lcd-receiver
```

Fazer upload do receptor LCD ESP32-S3:

```sh
cd firmware/receiver-lcd
pio run -e esp32-s3-lcd-receiver -t upload
```

Fazer upload em uma porta específica:

```sh
pio run -e esp32-s3-lcd-receiver -t upload --upload-port COM6
```

Abrir o monitor serial do receptor LCD:

```sh
pio device monitor -b 115200
```

## ESP-NOW para LCD separado

ESP-NOW é usado quando o LCD local fica em uma segunda placa ESP32-S3. Ele não
é a finalidade do projeto, mas uma forma prática de levar os dados do gateway
até o display sem roteador, SSID, senha ou broker.

Nesse caso, receptor e transmissor precisam usar o mesmo canal ESP-NOW.

A configuração do gateway é `ESP_NOW_WIFI_CHANNEL` em
`firmware/gateway/platformio.ini`. O receptor tem a mesma configuração em
`firmware/receiver-lcd/platformio.ini`. O valor padrão é o canal `1` nos dois
firmwares.

Se mudar o canal, altere nos dois firmwares:

```ini
build_flags =
	-D ESP_NOW_WIFI_CHANNEL=6
```

Se o receptor LCD continuar mostrando `Sem dados`, confira primeiro:

- o gateway está ligado e lendo a bateria
- as duas placas usam o mesmo canal ESP-NOW
- o receptor recebeu o firmware do receptor, não o firmware do gateway
- o endereço I2C do LCD está correto (`0x27` e `0x3F` são comuns)

## Gateway LCD direto, microSD e MQTT

O ambiente `esp32-s3-gateway-lcd-direct` usa uma única placa ESP32-S3 conectada
ao RS485 da bateria e ao LCD I2C. Esse é o caminho principal para transformar a
leitura da bateria em informação local e remota. Nesse modo o ESP-NOW fica
desativado, e o firmware também grava cada leitura do BMS no microSD em JSON
Lines:

```text
/bms_log.jsonl
```

Cada linha é um objeto JSON independente com o schema
`solmar.bms.reading.v1`. Esse formato foi escolhido em vez de CSV porque as
leituras têm tipos diferentes e arrays de células/temperaturas. O mesmo objeto
JSON é usado como payload MQTT para o dashboard.

Pinagem configurada para o módulo microSD SPI:

| microSD | ESP32-S3 |
|---|---|
| `3v3` | `3V3` |
| `GND` | `GND` |
| `CS` | `GPIO7` |
| `MOSI` | `GPIO6` |
| `CLK` | `GPIO4` |
| `MISO` | `GPIO5` |

O botão de páginas do LCD foi movido para `GPIO10` para deixar o microSD nos
pinos SPI padrão definidos neste alvo ESP32-S3.

Os pinos ficam em `firmware/gateway/platformio.ini`:

```ini
-D LCD_PAGE_BUTTON_PIN=10
-D SD_LOG_USE_DEFAULT_SPI_PINS=1
-D SD_LOG_CS_PIN=7
-D SD_LOG_SCK_PIN=4
-D SD_LOG_MISO_PIN=5
-D SD_LOG_MOSI_PIN=6
```

Compilar o gateway com LCD direto e microSD:

```sh
cd firmware/gateway
pio run -e esp32-s3-gateway-lcd-direct
```

Fazer upload:

```sh
pio run -e esp32-s3-gateway-lcd-direct -t upload
```

Se o cartão não inicializar, o firmware continua lendo o BMS e atualizando o
LCD; o erro aparece no monitor serial com prefixo `[SD]`.

### Dashboard remoto por MQTT

O mesmo ambiente `esp32-s3-gateway-lcd-direct` tambem pode publicar cada leitura
em MQTT. A conexao WiFi usa WiFiManager: na primeira configuracao, ou se nao
houver credenciais salvas, o ESP abre o portal `Solmar-BMS-Setup` por ate 120
segundos. Se o WiFi nao for configurado, a leitura RS485, o LCD e o microSD
continuam funcionando.

Configuracao padrao em `firmware/gateway/platformio.ini`:

```ini
-D BMS_MQTT_ENABLE=1
-D BMS_MQTT_HOST=\"broker.hivemq.com\"
-D BMS_MQTT_PORT=1883
-D BMS_MQTT_TOPIC_BASE=\"solmar/bms/felicity-fla12171\"
```

Os payloads usam o mesmo schema JSON `solmar.bms.reading.v1` do log em microSD
e sao publicados como mensagens retidas nos topicos:

```text
solmar/bms/felicity-fla12171/readings/v1/<device_id>/<tipo>
```

A pagina em `dashboard/index.html` assina por WebSocket o filtro:

```text
solmar/bms/felicity-fla12171/readings/v1/+/+
```

O codigo MQTT foi separado em `firmware/gateway/src/bms_mqtt_publisher.cpp`.
Para implementar GSM no futuro, troque o backend de rede desse modulo por um
cliente compativel com `Client`, como TinyGSM, sem alterar o parser do BMS nem a
pagina.


## Fontes usadas

| Fonte | O que foi usado no projeto |
|---|---|
| [Smartsmurf/FelicityBMS2MQTT](https://github.com/Smartsmurf/FelicityBMS2MQTT) | Base principal do RS485/Modbus da BMS. Usado para os registradores `0xF80B`, `0x1302`, `0x131C` e `0x132A`, além das escalas de tensão, corrente, limites, células, temperaturas e flags. |
| [alexbenisch/felicity-bms](https://github.com/alexbenisch/felicity-bms) | Confirmou os comandos RS485 `0xF80B`, `0x1302` e `0x132A`, os offsets internos de tensão/corrente/SOC e o fallback `0x132A len 0x14` para ler 16 slots de célula + 4 temperaturas. |
| [mr-manuel/venus-os_dbus-serialbattery](https://github.com/mr-manuel/venus-os_dbus-serialbattery) | Não foi usado como mapa Modbus, mas ajudou a confirmar o comportamento da BMS Felicity: células em mV, temperaturas filtrando `0x7FFF` e existência de campos internos como versão, modelo, serial, warnings e faults via BLE. |
| [Manual Felicity FLA12171-EU](./FLA12171-EU%20User%20Guide%20-%20English.pdf) | Usado para validar o modelo da bateria, tensão nominal, faixa de operação, limites elétricos, comunicação RS485/CAN e pinagem do conector. |
