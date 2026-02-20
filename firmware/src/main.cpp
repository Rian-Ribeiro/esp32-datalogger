#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Pinos ---
#define DHT_PIN     4
#define VOLTAGE_PIN 34
#define CURRENT_PIN 35

// --- DHT11 ---
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// --- Calibração dos sensores analógicos ---
// ZMPT101B: fator de escala para converter leitura ADC em Volts RMS
// Ajuste VOLTAGE_SCALE conforme calibração com multímetro
#define VOLTAGE_SCALE    0.1f   // V/unidade ADC (ajustar em campo)
// SCT-013 (100A/50mA): fator para converter leitura ADC em Amperes RMS
// Ajuste CURRENT_SCALE conforme o modelo do SCT-013 utilizado
#define CURRENT_SCALE    0.05f  // A/unidade ADC (ajustar em campo)

// Tempo de amostragem para cálculo RMS (ms) — cobre >= 5 ciclos a 50Hz (100ms)
#define SAMPLE_WINDOW_MS 100

// --- Protótipos ---
float readRMS(int pin, float scale);
bool sendData(float temp, float humidity, float voltage, float current);
void connectWiFi();

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n[ESP32 Datalogger] Iniciando...");

    dht.begin();

    // ADC: resolução 12 bits (0–4095), atenuação para até ~3.3V
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    connectWiFi();
}

void loop() {
    static unsigned long lastSend = 0;

    if (millis() - lastSend >= SEND_INTERVAL_MS) {
        lastSend = millis();

        // Reconecta WiFi se necessário
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Reconectando...");
            connectWiFi();
        }

        // Leitura DHT11
        float temp     = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (isnan(temp) || isnan(humidity)) {
            Serial.println("[DHT11] Falha na leitura — pulando ciclo.");
            return;
        }

        // Leitura RMS dos sensores AC
        float voltage = readRMS(VOLTAGE_PIN, VOLTAGE_SCALE);
        float current = readRMS(CURRENT_PIN, CURRENT_SCALE);

        Serial.printf("[Leitura] Temp=%.1f°C  Umid=%.1f%%  Tensao=%.1fV  Corrente=%.2fA\n",
                      temp, humidity, voltage, current);

        bool ok = sendData(temp, humidity, voltage, current);
        Serial.printf("[HTTP] Envio %s\n", ok ? "OK" : "FALHOU");
    }
}

// Calcula o valor RMS de um sinal AC amostrado durante SAMPLE_WINDOW_MS
float readRMS(int pin, float scale) {
    unsigned long start = millis();
    float sumSq = 0.0f;
    long  count  = 0;

    while (millis() - start < SAMPLE_WINDOW_MS) {
        int raw = analogRead(pin);
        // Centraliza em torno do ponto médio do ADC (sinal AC sobreposto em DC)
        float centered = raw - 2048.0f;
        sumSq += centered * centered;
        count++;
        delayMicroseconds(100); // ~10kHz de amostragem
    }

    if (count == 0) return 0.0f;
    float rmsRaw = sqrt(sumSq / count);
    return rmsRaw * scale;
}

void connectWiFi() {
    Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Falha na conexao. Continuando sem WiFi.");
    }
}

bool sendData(float temp, float humidity, float voltage, float current) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + API_PATH;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["temp"]      = serialized(String(temp, 1));
    doc["humidity"]  = serialized(String(humidity, 1));
    doc["voltage"]   = serialized(String(voltage, 1));
    doc["current"]   = serialized(String(current, 2));
    doc["timestamp"] = millis();

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    http.end();

    return (code == 200 || code == 201);
}
