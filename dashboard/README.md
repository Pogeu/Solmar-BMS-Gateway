# Solmar BMS dashboard

Dashboard web estatico para visualizar os pacotes MQTT publicados pelo gateway
`esp32-c3-gateway-lcd-direct`.

Abra `index.html` no navegador e conecte usando os valores padrao:

```text
Broker WebSocket: wss://broker.hivemq.com:8884/mqtt
Filtro de topico: solmar/bms/felicity-fla12171/readings/v1/+/+
```

O ESP publica no mesmo broker por MQTT TCP:

```text
Host: broker.hivemq.com
Porta: 1883
Topicos retidos: solmar/bms/felicity-fla12171/readings/v1/<device_id>/<tipo>
```

Tipos publicados atualmente:

- `battery_info`
- `cell_voltages`
- `charge_discharge`
- `version_info`

Se trocar o broker ou o topico no firmware, ajuste os campos da pagina. Para
broker publico, prefira usar um prefixo de topico unico por instalacao para
evitar conflito com outros usuarios.
