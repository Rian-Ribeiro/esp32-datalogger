# LoggerMTZ — Datalogger para Quadro Elétrico

Datalogger autônomo baseado em ESP32 para monitoramento de quadros elétricos. Mede temperatura, umidade, tensão AC e corrente AC, armazena o histórico internamente e serve um dashboard web diretamente — sem internet, sem servidor externo.

## Como funciona

O ESP32 cria um ponto de acesso WiFi próprio (`LoggerMTZ-XXXXXX`). Qualquer dispositivo conectado a essa rede acessa o dashboard em `http://192.168.4.1`.

```
[DHT11 / ZMPT101B / SCT-013]
        |
      ESP32 — Access Point WiFi "LoggerMTZ-XXXXXX"
        |
   Web Server (porta 80)
        ├── GET /          → Dashboard HTML (LittleFS)
        ├── GET /api/data  → Histórico JSON
        ├── GET /api/status→ Identidade do logger
        └── WS  /ws        → Stream em tempo real
```

## Hardware

| Sensor    | Grandeza            | GPIO |
|-----------|---------------------|------|
| DHT11     | Temperatura/Umidade | 4    |
| ZMPT101B  | Tensão AC (≤260V)   | 34   |
| SCT-013   | Corrente AC         | 35   |

## Estrutura

```
esp32-datalogger/
├── firmware/
│   ├── platformio.ini      ← build + libs
│   ├── src/
│   │   ├── main.cpp        ← firmware (AP + servidor + sensores)
│   │   ├── config.h        ← parâmetros configuráveis
│   │   └── config.h.example
│   └── data/
│       └── index.html      ← dashboard (gravado no LittleFS)
├── backend/                ← servidor Node.js alternativo
├── frontend/               ← dashboard React alternativo
└── docs/
    └── LOGGER-INFO.md      ← guia completo de uso e técnico
```

## Deploy rápido

```sh
cd firmware

# 1. Gravar firmware
pio run --target upload

# 2. Gravar dashboard no LittleFS
pio run --target uploadfs

# 3. Monitorar serial (confirma SSID e IP)
pio device monitor
```

## Acesso

1. Conecte ao WiFi `LoggerMTZ-XXXXXX` com senha `mtz1234567`
2. Abra `http://192.168.4.1` no navegador

Para detalhes completos, consulte [`docs/LOGGER-INFO.md`](docs/LOGGER-INFO.md).
