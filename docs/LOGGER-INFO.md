# LoggerMTZ — Guia do Usuário e Técnico

## O que é o LoggerMTZ?

O LoggerMTZ é um datalogger portátil para monitoramento de quadros elétricos. Ele mede temperatura interna, umidade, tensão AC e corrente AC em tempo real, armazena o histórico de leituras e disponibiliza um dashboard web acessível diretamente pelo smartphone ou notebook — **sem necessidade de internet ou servidor externo**.

---

## Como usar

### 1. Ligar o equipamento

Conecte o LoggerMTZ à alimentação (5V via USB ou fonte dedicada). O LED interno pisca brevemente durante a inicialização.

### 2. Conectar ao WiFi do logger

O logger cria automaticamente uma rede WiFi:

| Campo  | Valor                         |
|--------|-------------------------------|
| Rede   | `LoggerMTZ-XXXXXX`            |
| Senha  | `mtz1234567`                  |

> O sufixo `XXXXXX` é único por unidade — derivado do endereço MAC do chip ESP32. Você pode identificá-lo via monitor serial na primeira inicialização.

**No smartphone:** Configurações → WiFi → selecione a rede `LoggerMTZ-...`
**No computador:** clique no ícone de WiFi e selecione a rede.

### 3. Acessar o dashboard

Com o dispositivo conectado à rede do logger, abra qualquer navegador e acesse:

```
http://192.168.4.1
```

O dashboard carrega automaticamente com os dados em tempo real e o histórico de leituras desde o último boot (ou desde o último ciclo de armazenamento).

---

## O que o dashboard mostra

### Cards de valores instantâneos

| Card         | Descrição                                |
|--------------|------------------------------------------|
| Temperatura  | Temperatura interna do quadro (°C)       |
| Umidade      | Umidade relativa interna (%)             |
| Tensão       | Tensão AC da rede elétrica (V)           |
| Corrente     | Corrente AC da carga monitorada (A)      |

### Gráficos históricos

Três gráficos de linha mostram a evolução das grandezas ao longo do tempo:
- Tensão AC
- Corrente AC
- Temperatura e Umidade (sobrepostos)

O eixo X mostra os timestamps das leituras; o eixo Y é auto-ajustado ao intervalo dos dados.

### Indicadores de status

- **Ponto verde no header:** WebSocket conectado — dados chegando em tempo real.
- **Ponto vermelho piscando:** Sem conexão — o navegador tentará reconectar automaticamente a cada 3 segundos.
- **Contador de uptime:** Tempo desde o boot do logger.

### Atualização

Novas leituras chegam automaticamente via WebSocket a cada 5 segundos, sem necessidade de recarregar a página.

---

## Especificações técnicas

| Item              | Detalhe                                      |
|-------------------|----------------------------------------------|
| Microcontrolador  | ESP32-WROOM-32D                              |
| Sensor T/U        | DHT11 (GPIO 4)                               |
| Sensor de tensão  | ZMPT101B — até 260V AC (GPIO 34, ADC)        |
| Sensor de corrente| SCT-013 — até 100A AC (GPIO 35, ADC)         |
| Cálculo AC        | RMS por amostragem de 100ms (~5 ciclos 50Hz) |
| Intervalo leitura | 5 segundos                                   |
| Histórico RAM     | Últimas 200 leituras                         |
| Persistência      | LittleFS (flash interno) — até 200KB         |
| Rede WiFi         | 802.11 b/g/n — modo Access Point             |
| IP do logger      | 192.168.4.1 (fixo)                           |
| Porta HTTP        | 80                                           |

---

## Identificação de múltiplos loggers

Cada unidade LoggerMTZ possui um serial único gerado automaticamente a partir dos 3 últimos bytes do MAC do chip ESP32. Isso garante que, mesmo em ambientes com vários loggers, cada rede WiFi tenha um nome distinto.

**Exemplo:** `LoggerMTZ-D4F1AB`, `LoggerMTZ-C30E72`, `LoggerMTZ-A91B04`

Para descobrir o serial de uma unidade, conecte via cabo USB e abra o monitor serial (115200 baud). Na inicialização o firmware imprime:

```
=== LoggerMTZ ===
[FS] LittleFS montado
[FS] 85 leituras carregadas do histórico
[AP] SSID  : LoggerMTZ-D4F1AB
[AP] Senha : mtz1234567
[AP] IP    : 192.168.4.1
[HTTP] Servidor em http://192.168.4.1
```

---

## Armazenamento de dados

As leituras são salvas em formato JSON Lines no arquivo `/data.jsonl` dentro do LittleFS. Quando o arquivo ultrapassa 200KB, ele é zerado automaticamente e o registro recomeça — isso evita corrupção por overflow de flash.

O buffer em RAM mantém sempre as **últimas 200 leituras** disponíveis no dashboard, mesmo após a rotação do arquivo.

---

## Guia do desenvolvedor

### Pré-requisitos

- [PlatformIO](https://platformio.org/) (VS Code ou CLI)
- Python 3.x (gerenciado pelo PlatformIO)
- Cabo USB-Serial compatível com ESP32

### Deploy em uma nova unidade

```bash
cd firmware

# 1. Compilar e gravar o firmware
pio run --target upload

# 2. Gravar o sistema de arquivos (dashboard HTML)
pio run --target uploadfs

# 3. Monitorar o serial para confirmar a inicialização
pio device monitor
```

> Os dois uploads são independentes e podem ser feitos em qualquer ordem. O `uploadfs` só precisa ser repetido se o arquivo `data/index.html` for alterado.

### Alterar a senha do WiFi

Edite `firmware/src/config.h`:

```cpp
#define AP_PASSWORD  "nova_senha"
```

### Calibrar os sensores analógicos

Os fatores de escala em `config.h` precisam ser ajustados com um multímetro de referência após a instalação física:

```cpp
#define VOLTAGE_SCALE  0.1f   // multiplicador para tensão RMS
#define CURRENT_SCALE  0.05f  // multiplicador para corrente RMS
```

**Procedimento:**
1. Meça a tensão real com multímetro → anote `V_real`
2. Leia o valor exibido no dashboard → anote `V_lido`
3. Novo fator: `VOLTAGE_SCALE = VOLTAGE_SCALE_atual × (V_real / V_lido)`
4. Refaça o upload do firmware

### Estrutura do repositório

```
esp32-datalogger/
├── firmware/
│   ├── platformio.ini     ← configuração de build e libs
│   ├── src/
│   │   ├── main.cpp       ← firmware principal (AP, sensores, HTTP, WS)
│   │   ├── config.h       ← parâmetros configuráveis
│   │   └── config.h.example
│   └── data/
│       └── index.html     ← dashboard (gravado no LittleFS)
├── backend/               ← servidor Node.js alternativo (modo server)
├── frontend/              ← dashboard React alternativo (modo server)
└── docs/
    └── LOGGER-INFO.md     ← este arquivo
```

### API HTTP (acessível via 192.168.4.1)

| Método | Rota         | Descrição                              |
|--------|--------------|----------------------------------------|
| GET    | `/`          | Dashboard HTML                         |
| GET    | `/api/data`  | JSON array com histórico de leituras   |
| GET    | `/api/status`| Identidade e uptime do logger          |
| WS     | `/ws`        | Stream em tempo real (WebSocket)       |

#### Exemplo de resposta — `/api/data`

```json
[
  {"t":28.5,"h":65.0,"v":220.3,"c":1.45,"s":123},
  {"t":28.6,"h":64.8,"v":219.8,"c":1.47,"s":128}
]
```

| Campo | Tipo  | Descrição                   |
|-------|-------|-----------------------------|
| `t`   | float | Temperatura (°C)            |
| `h`   | float | Umidade (%)                 |
| `v`   | float | Tensão AC (V)               |
| `c`   | float | Corrente AC (A)             |
| `s`   | int   | Timestamp (segundos de boot)|

#### Exemplo de mensagem WebSocket — nova leitura

```json
{"type":"reading","t":28.5,"h":65.0,"v":220.3,"c":1.45,"s":130}
```

#### Exemplo de mensagem WebSocket — histórico (ao conectar)

```json
{"type":"history","data":[...array de leituras...]}
```
