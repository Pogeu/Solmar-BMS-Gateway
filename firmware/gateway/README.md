# Firmware Gateway

Lê os valores do BMS Felicity/Felicity ESS via RS485 Modbus e transforma esses
dados em informação para o usuário local e para a equipe remota.

Este projeto tem dois ambientes principais neste diretório:

- `esp32-s3-gateway`: lê a BMS e transmite um resumo por ESP-NOW para um display em
  outra placa ESP32-S3.
- `esp32-s3-gateway-lcd-direct`: lê a BMS, atualiza um display conectado na mesma
  placa, grava JSON Lines no microSD e publica MQTT para o dashboard.

ESP-NOW é uma opção de transporte local para separar a placa do gateway da
placa do LCD. O objetivo maior do firmware é servir como gateway entre a BMS e
as interfaces de uso do projeto.

Compilar o gateway ESP32-S3 com saída ESP-NOW:

```sh
pio run -e esp32-s3-gateway
```

Fazer upload do gateway ESP32-S3:

```sh
pio run -e esp32-s3-gateway -t upload
```

Se mais de uma placa estiver conectada, liste as portas e escolha uma
explicitamente:

```sh
pio device list
pio run -e esp32-s3-gateway -t upload --upload-port COM5
```

Abrir o monitor serial:

```sh
pio device monitor -b 9600
```

## Modo direto RS485 -> display 128x64 SPI -> MQTT

O ambiente `esp32-s3-gateway-lcd-direct` le a bateria Felicity/Felicity ESS pelo
mesmo barramento RS485 do gateway, mas nao usa ESP-NOW. Os dados sao escritos
direto no display grafico ST7565 128x64, gravados no microSD e publicados em MQTT para o
dashboard remoto.

Compilar:

```sh
pio run -e esp32-s3-gateway-lcd-direct
```

Fazer upload:

```sh
pio run -e esp32-s3-gateway-lcd-direct -t upload
```

Se precisar escolher a porta:

```sh
pio run -e esp32-s3-gateway-lcd-direct -t upload --upload-port COM5
```

Ligacoes padrao do ESP32-S3 neste alvo:

| Funcao | ESP32-S3 |
| --- | --- |
| RS485 RO / RX | GPIO0 |
| RS485 DI / TX | GPIO2 |
| RS485 DE + RE | GPIO1 |
| Display SCL / SPI SCK | GPIO4 |
| Display SI / SPI MOSI | GPIO6 |
| Display CS | GPIO15 |
| Display RS / DC | GPIO16 |
| Display RSE / RESET | GPIO17 |
| microSD CLK / SPI SCK | GPIO4 |
| microSD MOSI | GPIO6 |
| microSD MISO | GPIO5 |
| microSD CS | GPIO7 |
| Botao de pagina | GPIO10 para GND |

Os pinos auxiliares `IC_SCL`, `IC_CS`, `IC_SO` e `IC_SI` do modulo grafico nao
entram na comunicacao principal do firmware.

Display e microSD agora compartilham o mesmo barramento SPI fisico. O display
usa `SCK` e `MOSI` com `CS` proprio, enquanto o microSD usa os mesmos `SCK` e
`MOSI`, mais `MISO` e seu proprio `CS`.

Paginas do botao:

| Pagina | Conteudo |
| --- | --- |
| 0 | SOC e potencia com destaque + barra de progresso |
| 1 | Tensao, corrente, potencia e SOC |
| 2 | Comparacao SOC BMS x estimativa LiFePO4 por tensao |
| 3 | Temperatura e status de carga/descarga |
| 4 | Falhas, min/max das celulas e idade do ultimo pacote |

## Teste local das telas

Existe um ambiente separado para testar o display sem bateria ligada. Ele
simula mensagens `BmsMessage` com variacao de tensao, corrente, SOC,
temperatura e falhas para exercitar as cinco paginas do display.

Compilar:

```sh
pio run -e esp32-s3-gateway-display-test
```

Fazer upload:

```sh
pio run -e esp32-s3-gateway-display-test -t upload
```

Abrir o monitor serial:

```sh
pio device monitor -b 9600
```

Nesse modo o firmware nao inicia RS485, microSD nem MQTT. O botao de pagina em
`GPIO10` continua ativo e o monitor serial imprime os valores simulados.

Compilar o teste unitário do pacote:

```sh
pio test -e espnow-packet-test --without-uploading --without-testing
```

Rodar o teste unitário em um ESP32-S3 conectado:

```sh
pio test -e espnow-packet-test
```

O pacote binário ESP-NOW compartilhado com o receptor fica em
`../../shared/espnow_battery_packet.h`. O payload MQTT do modo direto usa o
schema JSON `solmar.bms.reading.v1`.
