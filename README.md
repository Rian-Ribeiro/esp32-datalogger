# esp32-teste

Projeto de teste para ESP32-WROOM-32D usando PlatformIO (framework Arduino).

O firmware pisca o LED interno (GPIO 2) a cada 1 segundo e imprime o estado via Serial.

## Requisitos

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html)
- ESP32-WROOM-32D conectado via USB (`/dev/ttyUSB0`)

## Uso

```bash
# Compilar e flashar
pio run --target upload

# Monitor serial
pio device monitor
```
