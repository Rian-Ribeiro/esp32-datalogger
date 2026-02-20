#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Hardware ---
DHT dht(DHT_PIN, DHT11);

// --- Rede ---
DNSServer       dnsServer;
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");

// --- SSID gerado em runtime a partir do MAC ---
String apSSID;

// --- Buffer circular de leituras em RAM ---
struct Reading {
    float    temp;
    float    humidity;
    float    voltage;
    float    current;
    uint32_t ts;        // segundos desde boot
};

Reading  history[MAX_HISTORY];
int      histCount = 0;
int      histHead  = 0;

// ============================================================
//  WiFi — Access Point
// ============================================================
void setupAP() {
    WiFi.mode(WIFI_AP);

    IPAddress localIP(AP_LOCAL_IP);
    IPAddress gateway(AP_GATEWAY);
    IPAddress subnet(AP_SUBNET);
    WiFi.softAPConfig(localIP, gateway, subnet);

    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char serial[7];
    snprintf(serial, sizeof(serial), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    apSSID = String("LoggerMTZ-") + serial;

    WiFi.softAP(apSSID.c_str(), AP_PASSWORD);

    // DNS: resolve qualquer domínio para o IP do logger (captive portal)
    dnsServer.start(53, "*", localIP);

    Serial.printf("\n[AP] SSID  : %s\n",   apSSID.c_str());
    Serial.printf("[AP] Senha : %s\n",      AP_PASSWORD);
    Serial.printf("[AP] IP    : %s\n",      WiFi.softAPIP().toString().c_str());
}

// ============================================================
//  LittleFS
// ============================================================
void setupFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] ERRO: falha ao montar LittleFS");
        return;
    }
    Serial.println("[FS] LittleFS montado");

    // Lista arquivos disponíveis (diagnóstico)
    File root = LittleFS.open("/");
    File f    = root.openNextFile();
    while (f) {
        Serial.printf("[FS] %s (%u bytes)\n", f.name(), f.size());
        f = root.openNextFile();
    }
}

void loadHistory() {
    if (!LittleFS.exists(DATA_FILE)) return;

    File f = LittleFS.open(DATA_FILE, "r");
    if (!f) return;

    histCount = 0;
    histHead  = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument doc;
        if (deserializeJson(doc, line) != DeserializationError::Ok) continue;

        Reading r;
        r.temp     = doc["t"] | 0.0f;
        r.humidity = doc["h"] | 0.0f;
        r.voltage  = doc["v"] | 0.0f;
        r.current  = doc["c"] | 0.0f;
        r.ts       = doc["s"] | (uint32_t)0;

        history[histHead] = r;
        histHead = (histHead + 1) % MAX_HISTORY;
        if (histCount < MAX_HISTORY) histCount++;
    }
    f.close();

    Serial.printf("[FS] %d leituras carregadas do histórico\n", histCount);
}

void saveReading(const Reading& r) {
    File chk = LittleFS.open(DATA_FILE, "r");
    if (chk && chk.size() > (size_t)(MAX_FILE_KB * 1024)) {
        chk.close();
        LittleFS.remove(DATA_FILE);
        Serial.println("[FS] Arquivo rotacionado");
    } else if (chk) {
        chk.close();
    }

    File f = LittleFS.open(DATA_FILE, "a");
    if (!f) return;

    char line[80];
    snprintf(line, sizeof(line),
             "{\"t\":%.1f,\"h\":%.1f,\"v\":%.1f,\"c\":%.2f,\"s\":%lu}\n",
             r.temp, r.humidity, r.voltage, r.current, (unsigned long)r.ts);
    f.print(line);
    f.close();
}

// ============================================================
//  Buffer circular
// ============================================================
void addReading(const Reading& r) {
    history[histHead] = r;
    histHead = (histHead + 1) % MAX_HISTORY;
    if (histCount < MAX_HISTORY) histCount++;
}

String historyToJSON() {
    String out = "[";
    int start = (histCount < MAX_HISTORY) ? 0 : histHead;

    for (int i = 0; i < histCount; i++) {
        const Reading& r = history[(start + i) % MAX_HISTORY];
        if (i > 0) out += ',';
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "{\"t\":%.1f,\"h\":%.1f,\"v\":%.1f,\"c\":%.2f,\"s\":%lu}",
                 r.temp, r.humidity, r.voltage, r.current, (unsigned long)r.ts);
        out += buf;
    }
    out += ']';
    return out;
}

// ============================================================
//  WebSocket
// ============================================================
void broadcastReading(const Reading& r) {
    char buf[100];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"reading\",\"t\":%.1f,\"h\":%.1f,\"v\":%.1f,\"c\":%.2f,\"s\":%lu}",
             r.temp, r.humidity, r.voltage, r.current, (unsigned long)r.ts);
    ws.textAll(buf);
}

void onWSEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
               AwsEventType type, void*, uint8_t*, size_t) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Cliente #%u conectado\n", client->id());
        String msg = "{\"type\":\"history\",\"data\":" + historyToJSON() + "}";
        client->text(msg);
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Cliente #%u desconectado\n", client->id());
    }
}

// ============================================================
//  HTTP Server
// ============================================================
void serveIndex(AsyncWebServerRequest* req) {
    if (LittleFS.exists("/index.html")) {
        req->send(LittleFS, "/index.html", "text/html");
    } else {
        req->send(200, "text/html",
            "<!DOCTYPE html><html><body>"
            "<h2>LoggerMTZ</h2>"
            "<p style='color:red'>Arquivo index.html nao encontrado no LittleFS.<br>"
            "Execute: <code>pio run --target uploadfs</code></p>"
            "</body></html>");
    }
}

void setupServer() {
    // Dashboard
    server.on("/",           HTTP_GET, serveIndex);
    server.on("/index.html", HTTP_GET, serveIndex);

    // API: histórico
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", historyToJSON());
    });

    // API: status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        char buf[200];
        snprintf(buf, sizeof(buf),
                 "{\"ssid\":\"%s\",\"ip\":\"%s\",\"uptime\":%lu,\"readings\":%d}",
                 apSSID.c_str(),
                 WiFi.softAPIP().toString().c_str(),
                 millis() / 1000,
                 histCount);
        req->send(200, "application/json", buf);
    });

    // Captive portal + catch-all → dashboard
    server.onNotFound([](AsyncWebServerRequest* req) {
        // Requisições de detecção de conectividade (Android/iOS/Windows)
        // também recebem o dashboard, ativando o "Abrir na rede"
        if (LittleFS.exists("/index.html")) {
            req->send(LittleFS, "/index.html", "text/html");
        } else {
            req->redirect("/");
        }
    });

    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    server.begin();

    Serial.printf("[HTTP] Servidor em http://%s\n", WiFi.softAPIP().toString().c_str());
}

// ============================================================
//  Sensores
// ============================================================
float readRMS(int pin, float scale) {
    unsigned long start = millis();
    float sumSq = 0.0f;
    long  count  = 0;

    while (millis() - start < SAMPLE_WINDOW) {
        float centered = (float)analogRead(pin) - 2048.0f;
        sumSq += centered * centered;
        count++;
        delayMicroseconds(100);
    }

    return count > 0 ? sqrtf(sumSq / count) * scale : 0.0f;
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== LoggerMTZ ===");

    dht.begin();
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    setupFS();
    loadHistory();
    setupAP();
    setupServer();
}

void loop() {
    static unsigned long lastRead = 0;

    dnsServer.processNextRequest();
    ws.cleanupClients();

    if (millis() - lastRead >= READ_INTERVAL) {
        lastRead = millis();

        float temp     = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (isnan(temp) || isnan(humidity)) {
            Serial.println("[DHT11] Falha na leitura — pulando ciclo");
            return;
        }

        Reading r;
        r.temp     = temp;
        r.humidity = humidity;
        r.voltage  = readRMS(VOLTAGE_PIN, VOLTAGE_SCALE);
        r.current  = readRMS(CURRENT_PIN, CURRENT_SCALE);
        r.ts       = millis() / 1000;

        addReading(r);
        saveReading(r);
        broadcastReading(r);

        Serial.printf("[Leitura] Temp=%.1f°C  Umid=%.1f%%  Tensao=%.1fV  Corrente=%.2fA\n",
                      r.temp, r.humidity, r.voltage, r.current);
    }
}
