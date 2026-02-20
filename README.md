# ESP32 Datalogger — Monitoramento de Quadro Elétrico

Datalogger para quadro elétrico com ESP32, medindo temperatura, umidade, tensão AC e corrente AC, com dashboard web em tempo real.

## Hardware

| Sensor    | Grandeza           | GPIO |
|-----------|--------------------|------|
| DHT11     | Temperatura/Umidade| 4    |
| ZMPT101B  | Tensão AC (≤260V)  | 34   |
| SCT-013   | Corrente AC        | 35   |

## Estrutura

```
esp32-datalogger/
├── firmware/      ← PlatformIO (ESP32)
├── backend/       ← Node.js + Express + SQLite + WebSocket
└── frontend/      ← React + Recharts
```

## Setup

### Firmware

1. Copie e configure as credenciais:
   ```sh
   cp firmware/src/config.h.example firmware/src/config.h
   # Edite WIFI_SSID, WIFI_PASSWORD e SERVER_HOST
   ```
2. Compile e faça upload:
   ```sh
   cd firmware
   pio run --target upload
   pio device monitor   # verifica conexão e envio
   ```

### Backend

```sh
cd backend
npm install
npm start
# Servidor em http://localhost:3000
```

### Frontend

```sh
cd frontend
npm install
npm run dev
# Dashboard em http://localhost:5173
```

## API

| Método | Rota           | Descrição                        |
|--------|----------------|----------------------------------|
| POST   | `/api/data`    | Recebe leitura do ESP32 (JSON)   |
| GET    | `/api/data`    | Retorna últimas N leituras       |

### Payload POST `/api/data`

```json
{
  "temp": 28.5,
  "humidity": 65.0,
  "voltage": 220.3,
  "current": 1.45
}
```

## Calibração dos Sensores Analógicos

Os fatores `VOLTAGE_SCALE` e `CURRENT_SCALE` em `config.h` precisam ser ajustados com um multímetro de referência após a montagem. Os valores padrão são estimativas iniciais.

## WebSocket

O backend faz broadcast de cada nova leitura (`{ type: "reading", data: {...} }`) para todos os clientes conectados. Ao conectar, o cliente recebe o histórico recente (`{ type: "history", data: [...] }`).
