# Firmware Gateway

Lê os valores do BMS Felicity/Felicity ESS via RS485 Modbus e transforma esses
dados em informação para o usuário local e para a equipe remota.

Este projeto tem dois ambientes principais neste diretório:

- `esp32-s3-gateway`: lê a BMS e transmite um resumo por ESP-NOW para um LCD em
  outra placa ESP32-S3.
- `esp32-s3-gateway-lcd-direct`: lê a BMS, atualiza um LCD conectado na mesma
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

## Modo direto RS485 -> LCD 16x2 -> MQTT

O ambiente `esp32-s3-gateway-lcd-direct` le a bateria Felicity/Felicity ESS pelo
mesmo barramento RS485 do gateway, mas nao usa ESP-NOW. Os dados sao escritos
direto no LCD I2C 16x2, gravados no microSD e publicados em MQTT para o
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
| LCD SDA | GPIO8 |
| LCD SCL | GPIO9 |
| Botao de pagina | GPIO4 para GND |

O LCD usa endereco I2C `0x27` por padrao. Se o seu modulo estiver em outro
endereco, ajuste `LCD_I2C_ADDR` no `platformio.ini`.

Paginas do botao:

| Pagina | Conteudo |
| --- | --- |
| 0 | SOC e potencia em numeros grandes |
| 1 | Tensao, corrente, potencia e SOC |
| 2 | Comparacao SOC BMS x estimativa LiFePO4 por tensao |
| 3 | Temperatura e status de carga/descarga |
| 4 | Falhas, min/max das celulas e idade do ultimo pacote |

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
