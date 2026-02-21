# LoggerMTZ — Guia do Usuário e Técnico

Sistema de monitoramento trifásico para inversores fotovoltaicos de até 75kW.
Mede tensão, corrente, potência e temperatura diretamente nos cabos do quadro elétrico —
sem comunicação com o inversor, compatível com qualquer fabricante.

---

## Como usar

### 1. Ligar o equipamento

Conecte o LoggerMTZ à alimentação (5V via USB ou fonte dedicada).

### 2. Conectar ao WiFi do logger

| Campo | Valor |
|-------|-------|
| Rede  | `LoggerMTZ-XXXXXX` (sufixo único por unidade) |
| Senha | `mtz1234567` |

**Smartphone:** Configurações → WiFi → selecione a rede `LoggerMTZ-...`

### 3. Acessar o dashboard

```
http://192.168.4.1
```

---

## O que o dashboard mostra

| Elemento | Descrição |
|----------|-----------|
| Potência Total | kVA somado das 3 fases, com badge GERANDO/AGUARDANDO |
| L1 / L2 / L3 Tensão | Tensão RMS fase-neutro (V) |
| L1 / L2 / L3 Corrente | Corrente RMS por fase (A) |
| L1 / L2 / L3 Potência | Potência aparente por fase (kVA) |
| Energia Acumulada | kWh somado desde o último reset |
| Temperatura / Umidade | Ambiente interno do quadro |
| Gráficos | Tensões, Correntes, Potência Total e Temp/Umidade |
| Alertas | Banner vermelho piscante para condições anormais |

---

## Hardware

| Componente | Modelo | Qtd | Função |
|------------|--------|-----|--------|
| Microcontrolador | DOIT ESP32 DevKit V1 | 1 | Processamento e WiFi |
| Sensor de corrente | SCT-013-300 | 3 | Corrente AC não invasiva (300A:50mA) |
| Sensor de tensão | ZMPT101B | 3 | Tensão AC fase-neutro (~127V) |
| Sensor T/U | DHT11 | 1 | Temperatura e umidade do quadro |
| Resistor de burden | 33Ω | 3 | Nos SCT-013-300 |
| Circuito de offset | 10kΩ + 10kΩ | 6 | Centraliza sinais AC em 1.65V (VCC/2) |

---

## Mapeamento de pinos — somente ADC1

> **CRÍTICO:** O ADC2 do ESP32 fica inoperante quando o WiFi está ativo.
> Todos os pinos analógicos usam exclusivamente o **ADC1**.

| GPIO | Nome | Canal ADC | Função |
|------|------|-----------|--------|
| 36 | VP | ADC1_CH0 | Tensão L1 (ZMPT101B) |
| 39 | VN | ADC1_CH3 | Tensão L2 (ZMPT101B) |
| 34 | IO34 | ADC1_CH6 | Tensão L3 (ZMPT101B) |
| 32 | IO32 | ADC1_CH4 | Corrente L1 (SCT-013) |
| 33 | IO33 | ADC1_CH5 | Corrente L2 (SCT-013) |
| 35 | IO35 | ADC1_CH7 | Corrente L3 (SCT-013) |
| 4 | IO4 | Digital | DHT11 (dados) |

---

## Calibração dos sensores

### SCT-013-300 com burden 33Ω

```
Relação:   300A primário → 50mA secundário
Burden:    33Ω
Pico:      0.05A × 33Ω × √2 ≈ 2.33V  (dentro de 0~3.3V ✓)
RMS max:   0.05A × 33Ω = 1.65V RMS

Fórmula do fator:
  CURRENT_SCALE = 300 / (0.05 × 33 × 2048) ≈ 0.008839

Calibração em campo (com alicate amperímetro):
  novo_fator = CURRENT_SCALE × (A_medido / A_dashboard)
```

Edite `firmware/src/config.h`:
```cpp
#define CURRENT_SCALE   0.008839f   // ajustar após calibração
```

### ZMPT101B (tensão fase-neutro ~127V)

```
Fator empírico inicial: VOLTAGE_SCALE = 0.060

Calibração com multímetro:
  novo_fator = VOLTAGE_SCALE × (V_multimetro / V_dashboard)

Exemplo: dashboard = 110V, multímetro = 127V
  novo_fator = 0.060 × (127/110) ≈ 0.0693
```

Edite `firmware/src/config.h`:
```cpp
#define VOLTAGE_SCALE   0.060f   // ajustar após calibração
```

---

## Instalação dos sensores no quadro elétrico

### SCT-013-300 (corrente)
- Clipe na fase correspondente (L1, L2, L3) — **não invasivo**, não corta o cabo
- Resistor de burden 33Ω em paralelo entre os terminais do SCT
- Circuito de offset: divisor 10kΩ+10kΩ entre 3.3V e GND; ponto médio → ADC

### ZMPT101B (tensão)
- Conectar entre a fase e o neutro (127V fase-neutro)
- Usar fusível de proteção adequado
- Saída do módulo → circuito de offset → ADC

---

## Especificações técnicas

| Item | Detalhe |
|------|---------|
| Microcontrolador | ESP32-WROOM-32D |
| Sensor T/U | DHT11 (GPIO 4) |
| Sensores de tensão | ZMPT101B × 3 (GPIO 36, 39, 34 — ADC1) |
| Sensores de corrente | SCT-013-300 × 3 (GPIO 32, 33, 35 — ADC1) |
| Cálculo AC | RMS por amostragem livre de 500ms (~30 ciclos 60Hz) |
| Intervalo leitura | 5 segundos |
| Histórico RAM | Últimas 200 leituras (buffer circular) |
| Persistência | LittleFS — até 200KB com rotação e backup |
| Rede WiFi | 802.11 b/g/n — modo Access Point |
| IP do logger | 192.168.4.1 (fixo) |
| Porta HTTP | 80 |
| Inversor máximo | 75kW (~207A por fase a 220V trifásico) |

---

## Alertas automáticos

| Código | Condição | Ação |
|--------|----------|------|
| `OVERCURRENT` | Qualquer fase > 220A | Banner + WebSocket |
| `UNDERVOLTAGE` | Fase ativa < 100V | Banner + WebSocket |
| `OVERVOLTAGE` | Qualquer fase > 145V | Banner + WebSocket |
| `IMBALANCE` | Desequilíbrio > 10% (com média > 5A) | Banner + WebSocket |

---

## API HTTP (acessível em 192.168.4.1)

| Método | Rota | Descrição |
|--------|------|-----------|
| GET | `/` | Dashboard HTML |
| GET | `/api/data` | Histórico trifásico JSON |
| GET | `/api/status` | ssid, ip, uptime, readings, energyKwh, isGenerating |
| GET | `/api/energy` | `{"kwh": 123.45}` |
| POST | `/api/reset-energy` | Zera acumulador de energia |
| WS | `/ws` | Stream em tempo real |

### Formato JSON das leituras

```json
{
  "v1": 127.1, "v2": 126.8, "v3": 127.3,
  "i1": 185.2, "i2": 183.1, "i3": 186.4,
  "p1": 23.5,  "p2": 23.2,  "p3": 23.6,
  "pt": 70.3,  "t": 34.2,   "h": 58.0,
  "kwh": 142.5, "gen": true, "s": 3600
}
```

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `v1/v2/v3` | float | Tensão RMS por fase (V) |
| `i1/i2/i3` | float | Corrente RMS por fase (A) |
| `p1/p2/p3` | float | Potência por fase (kVA) |
| `pt` | float | Potência total (kVA) |
| `t` | float | Temperatura (°C) |
| `h` | float | Umidade (%) |
| `kwh` | float | Energia acumulada (kWh) |
| `gen` | bool | `true` se gerando (pt > 0.5 kVA) |
| `s` | int | Uptime em segundos |

### Formato de alerta WebSocket

```json
{"type": "alert", "code": "OVERCURRENT", "phase": "L1", "value": 225.3}
```

---

## Deploy em nova unidade

```bash
cd firmware

# 1. Copiar configuração
cp src/config.h.example src/config.h
# Editar config.h se necessário (senha, fatores de calibração)

# 2. Gravar firmware
pio run --target upload

# 3. Gravar dashboard no LittleFS
pio run --target uploadfs

# 4. Monitorar serial (confirma SSID e IP)
pio device monitor
```

Saída esperada na serial:
```
=== LoggerMTZ — Sistema Trifásico 75kW ===
[FS] LittleFS montado
[FS] index.html (12345 bytes)
[Energy] 0.000 kWh carregados
[AP] SSID  : LoggerMTZ-D4F1AB
[AP] Senha : mtz1234567
[AP] IP    : 192.168.4.1
[HTTP] Servidor em http://192.168.4.1
[Leitura] V: 127.1/126.8/127.3 V  I: 185.2/183.1/186.4 A  P: 70.3 kVA  T: 34.2°C H: 58.0%  E: 0.098 kWh  Gen: SIM
```

---

## Estrutura do repositório

```
esp32-datalogger/
├── firmware/
│   ├── platformio.ini          ← build, libs (portas auto-detectadas)
│   ├── src/
│   │   ├── main.cpp            ← firmware principal (trifásico)
│   │   ├── config.h            ← parâmetros configuráveis
│   │   └── config.h.example    ← template de configuração
│   └── data/
│       └── index.html          ← dashboard standalone (LittleFS, ~60KB)
├── backend/                    ← servidor Node.js (modo servidor)
│   ├── server.js               ← Express + WebSocket + SQLite
│   └── db.js                   ← schema trifásico
├── frontend/                   ← dashboard React (modo servidor)
│   └── src/
│       ├── App.jsx             ← WS com VITE_WS_URL configurável
│       └── components/Dashboard.jsx
└── docs/
    └── LOGGER-INFO.md          ← este arquivo
```

---

## Identificação de múltiplos loggers

Cada unidade tem SSID único derivado dos 3 últimos bytes do MAC:

```
LoggerMTZ-D4F1AB   LoggerMTZ-C30E72   LoggerMTZ-A91B04
```

Para descobrir o serial de uma unidade, conecte via USB e abra o monitor serial (115200 baud).
