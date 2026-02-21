#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================
//  Versão e modelo do firmware
// ============================================================
#define FIRMWARE_VERSION  "2.1.0"
#define FIRMWARE_MODEL    "LoggerMTZ Trifasico"

// --- Hardware ---
DHT dht(DHT_PIN, DHT11);

// --- Rede ---
DNSServer       dnsServer;
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");

// --- Identidade do logger ---
String apSSID;
String apSerial;               // 6 chars hex derivados do MAC (ex: 7EAD01)
String apPassword = AP_PASSWORD;

// --- Flag de reinício agendado ---
bool shouldRestart = false;

// ============================================================
//  Estrutura de leitura — sistema trifásico 75kW
// ============================================================
struct Reading {
    float    v1, v2, v3;
    float    i1, i2, i3;
    float    p1, p2, p3;
    float    pTotal;
    float    temp;
    float    humidity;
    uint32_t ts;
};

Reading  history[MAX_HISTORY];
int      histCount = 0;
int      histHead  = 0;

float energyKwh   = 0.0f;
bool  isGenerating = false;

// ============================================================
//  Config de rede — persistida em /apconfig.json
// ============================================================
void loadAPConfig() {
    if (!LittleFS.exists("/apconfig.json")) return;
    File f = LittleFS.open("/apconfig.json", "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        String s = doc["ssid"]     | "";
        String p = doc["password"] | "";
        if (s.length() >= 2)  apSSID     = s;
        if (p.length() >= 8)  apPassword = p;
        Serial.printf("[Config] SSID personalizado: %s\n", apSSID.c_str());
    }
    f.close();
}

void saveAPConfig(const String& ssid, const String& password) {
    File f = LittleFS.open("/apconfig.json", "w");
    if (!f) return;
    f.printf("{\"ssid\":\"%s\",\"password\":\"%s\"}\n",
             ssid.c_str(), password.c_str());
    f.close();
}

// ============================================================
//  Energia — persistida em /energy.json
// ============================================================
void loadEnergy() {
    if (!LittleFS.exists(ENERGY_FILE)) return;
    File f = LittleFS.open(ENERGY_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        energyKwh = doc["kwh"] | 0.0f;
        Serial.printf("[Energy] %.3f kWh carregados\n", energyKwh);
    }
    f.close();
}

void saveEnergy() {
    File f = LittleFS.open(ENERGY_FILE, "w");
    if (!f) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"kwh\":%.3f}\n", energyKwh);
    f.print(buf);
    f.close();
}

// ============================================================
//  WiFi — Access Point
// ============================================================
void setupAP() {
    WiFi.mode(WIFI_AP);

    IPAddress localIP(AP_LOCAL_IP);
    IPAddress gateway(AP_GATEWAY);
    IPAddress subnet(AP_SUBNET);
    WiFi.softAPConfig(localIP, gateway, subnet);

    // Serial único derivado do MAC
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char serial[7];
    snprintf(serial, sizeof(serial), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    apSerial = String(serial);

    // SSID padrão; substituído se /apconfig.json existir
    if (apSSID.isEmpty()) apSSID = String("LoggerMTZ-") + apSerial;

    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    dnsServer.start(53, "*", localIP);

    Serial.printf("\n[AP] SSID   : %s\n", apSSID.c_str());
    Serial.printf("[AP] Serial : %s\n",   apSerial.c_str());
    Serial.printf("[AP] IP     : %s\n",   WiFi.softAPIP().toString().c_str());
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
    File root = LittleFS.open("/");
    File f    = root.openNextFile();
    while (f) {
        Serial.printf("[FS] %s (%u bytes)\n", f.name(), f.size());
        f = root.openNextFile();
    }
}

// ============================================================
//  Histórico
// ============================================================
void loadHistory() {
    if (!LittleFS.exists(DATA_FILE)) return;
    File f = LittleFS.open(DATA_FILE, "r");
    if (!f) return;

    histCount = 0; histHead = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        JsonDocument doc;
        if (deserializeJson(doc, line) != DeserializationError::Ok) continue;

        Reading r;
        r.v1 = doc["v1"]|0.0f; r.v2 = doc["v2"]|0.0f; r.v3 = doc["v3"]|0.0f;
        r.i1 = doc["i1"]|0.0f; r.i2 = doc["i2"]|0.0f; r.i3 = doc["i3"]|0.0f;
        r.p1 = doc["p1"]|0.0f; r.p2 = doc["p2"]|0.0f; r.p3 = doc["p3"]|0.0f;
        r.pTotal   = doc["pt"]|0.0f;
        r.temp     = doc["t"] |0.0f;
        r.humidity = doc["h"] |0.0f;
        r.ts       = doc["s"] |(uint32_t)0;

        history[histHead] = r;
        histHead = (histHead + 1) % MAX_HISTORY;
        if (histCount < MAX_HISTORY) histCount++;
    }
    f.close();
    Serial.printf("[FS] %d leituras carregadas\n", histCount);
}

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
        char buf[200];
        snprintf(buf, sizeof(buf),
            "{\"v1\":%.1f,\"v2\":%.1f,\"v3\":%.1f,"
            "\"i1\":%.2f,\"i2\":%.2f,\"i3\":%.2f,"
            "\"p1\":%.2f,\"p2\":%.2f,\"p3\":%.2f,"
            "\"pt\":%.2f,\"t\":%.1f,\"h\":%.1f,\"s\":%lu}",
            r.v1,r.v2,r.v3, r.i1,r.i2,r.i3, r.p1,r.p2,r.p3,
            r.pTotal,r.temp,r.humidity,(unsigned long)r.ts);
        out += buf;
    }
    return out + ']';
}

// ============================================================
//  Rotação de arquivo com backup
// ============================================================
void saveReading(const Reading& r) {
    File chk = LittleFS.open(DATA_FILE, "r");
    if (chk && chk.size() > (size_t)(MAX_FILE_KB * 1024)) {
        chk.close();
        if (LittleFS.exists(DATA_BAK_FILE)) LittleFS.remove(DATA_BAK_FILE);
        LittleFS.rename(DATA_FILE, DATA_BAK_FILE);
        Serial.println("[FS] Arquivo rotacionado");
    } else if (chk) { chk.close(); }

    File f = LittleFS.open(DATA_FILE, "a");
    if (!f) return;
    char line[220];
    snprintf(line, sizeof(line),
        "{\"v1\":%.1f,\"v2\":%.1f,\"v3\":%.1f,"
        "\"i1\":%.2f,\"i2\":%.2f,\"i3\":%.2f,"
        "\"p1\":%.2f,\"p2\":%.2f,\"p3\":%.2f,"
        "\"pt\":%.2f,\"t\":%.1f,\"h\":%.1f,\"s\":%lu}\n",
        r.v1,r.v2,r.v3, r.i1,r.i2,r.i3, r.p1,r.p2,r.p3,
        r.pTotal,r.temp,r.humidity,(unsigned long)r.ts);
    f.print(line); f.close();
}

// ============================================================
//  Leitura RMS — loop livre
// ============================================================
float readRMS(int pin, float scale, float noiseFloor) {
    unsigned long start = millis();
    float sumSq = 0.0f; long count = 0;
    while (millis() - start < SAMPLE_WINDOW) {
        float c = (float)analogRead(pin) - 2048.0f;
        sumSq += c * c; count++;
    }
    if (count == 0) return 0.0f;
    float rms = sqrtf(sumSq / (float)count) * scale;
    return (rms < noiseFloor) ? 0.0f : rms;
}

// ============================================================
//  Alertas
// ============================================================
void broadcastAlert(const char* code, const char* phase, float value) {
    char buf[120];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"alert\",\"code\":\"%s\",\"phase\":\"%s\",\"value\":%.2f}",
        code, phase, value);
    ws.textAll(buf);
    Serial.printf("[ALERTA] %s — %s — %.2f\n", code, phase, value);
}

void checkAlerts(const Reading& r) {
    if (r.i1 > CURRENT_MAX_A) broadcastAlert("OVERCURRENT",  "L1", r.i1);
    if (r.i2 > CURRENT_MAX_A) broadcastAlert("OVERCURRENT",  "L2", r.i2);
    if (r.i3 > CURRENT_MAX_A) broadcastAlert("OVERCURRENT",  "L3", r.i3);

    if (r.v1 > VOLTAGE_NOISE_FLOOR && r.v1 < VOLTAGE_MIN_V) broadcastAlert("UNDERVOLTAGE","L1",r.v1);
    if (r.v2 > VOLTAGE_NOISE_FLOOR && r.v2 < VOLTAGE_MIN_V) broadcastAlert("UNDERVOLTAGE","L2",r.v2);
    if (r.v3 > VOLTAGE_NOISE_FLOOR && r.v3 < VOLTAGE_MIN_V) broadcastAlert("UNDERVOLTAGE","L3",r.v3);

    if (r.v1 > VOLTAGE_MAX_V) broadcastAlert("OVERVOLTAGE",  "L1", r.v1);
    if (r.v2 > VOLTAGE_MAX_V) broadcastAlert("OVERVOLTAGE",  "L2", r.v2);
    if (r.v3 > VOLTAGE_MAX_V) broadcastAlert("OVERVOLTAGE",  "L3", r.v3);

    float avg = (r.i1 + r.i2 + r.i3) / 3.0f;
    if (avg > 5.0f) {
        float imbal = (max({r.i1,r.i2,r.i3}) - min({r.i1,r.i2,r.i3})) / avg * 100.0f;
        if (imbal > IMBALANCE_PCT) broadcastAlert("IMBALANCE", "L1-L3", imbal);
    }
}

// ============================================================
//  WebSocket
// ============================================================
void broadcastReading(const Reading& r) {
    char buf[260];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"reading\","
        "\"v1\":%.1f,\"v2\":%.1f,\"v3\":%.1f,"
        "\"i1\":%.2f,\"i2\":%.2f,\"i3\":%.2f,"
        "\"p1\":%.2f,\"p2\":%.2f,\"p3\":%.2f,"
        "\"pt\":%.2f,\"t\":%.1f,\"h\":%.1f,"
        "\"kwh\":%.3f,\"gen\":%s,\"s\":%lu}",
        r.v1,r.v2,r.v3, r.i1,r.i2,r.i3, r.p1,r.p2,r.p3,
        r.pTotal,r.temp,r.humidity,
        energyKwh, isGenerating?"true":"false", (unsigned long)r.ts);
    ws.textAll(buf);
}

void onWSEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
               AwsEventType type, void*, uint8_t*, size_t) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Cliente #%u conectado\n", client->id());
        String msg = "{\"type\":\"history\",\"kwh\":" + String(energyKwh,3) +
                     ",\"data\":" + historyToJSON() + "}";
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
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/index.html", "text/html");
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        resp->addHeader("Pragma", "no-cache");
        resp->addHeader("Expires", "0");
        req->send(resp);
    } else {
        req->send(200,"text/html","<h2>LoggerMTZ</h2><p>Execute: pio run --target uploadfs</p>");
    }
}

void setupServer() {
    server.on("/",           HTTP_GET, serveIndex);
    server.on("/index.html", HTTP_GET, serveIndex);

    // Histórico trifásico
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", historyToJSON());
    });

    // Status básico
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        char buf[300];
        snprintf(buf, sizeof(buf),
            "{\"ssid\":\"%s\",\"ip\":\"%s\",\"uptime\":%lu,"
            "\"readings\":%d,\"energyKwh\":%.3f,\"isGenerating\":%s}",
            apSSID.c_str(), WiFi.softAPIP().toString().c_str(),
            millis()/1000, histCount, energyKwh, isGenerating?"true":"false");
        req->send(200, "application/json", buf);
    });

    // Informações completas do sistema
    server.on("/api/sysinfo", HTTP_GET, [](AsyncWebServerRequest* req) {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"version\":\"%s\",\"model\":\"%s\","
            "\"serial\":\"%s\",\"ssid\":\"%s\","
            "\"ip\":\"%s\",\"uptime\":%lu,"
            "\"heap\":%u,\"readings\":%d,"
            "\"energyKwh\":%.3f,\"isGenerating\":%s,"
            "\"clients\":%u,\"dbStatus\":\"autonomo\"}",
            FIRMWARE_VERSION, FIRMWARE_MODEL,
            apSerial.c_str(), apSSID.c_str(),
            WiFi.softAPIP().toString().c_str(),
            millis()/1000,
            (unsigned)ESP.getFreeHeap(),
            histCount,
            energyKwh,
            isGenerating?"true":"false",
            (unsigned)ws.count());
        req->send(200, "application/json", buf);
    });

    // Configuração atual da rede
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        char buf[200];
        snprintf(buf, sizeof(buf),
            "{\"ssid\":\"%s\",\"serial\":\"%s\"}",
            apSSID.c_str(), apSerial.c_str());
        req->send(200, "application/json", buf);
    });

    // Salvar nova configuração de rede (POST com JSON no body)
    server.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(400, "application/json", "{\"error\":\"JSON invalido\"}");
                return;
            }
            String newSSID = doc["ssid"]     | "";
            String newPass = doc["password"] | "";

            if (newSSID.length() < 2 || newSSID.length() > 32) {
                req->send(400, "application/json", "{\"error\":\"SSID: 2 a 32 caracteres\"}");
                return;
            }
            if (newPass.length() < 8 || newPass.length() > 63) {
                req->send(400, "application/json", "{\"error\":\"Senha: 8 a 63 caracteres\"}");
                return;
            }

            saveAPConfig(newSSID, newPass);
            req->send(200, "application/json", "{\"ok\":true,\"restart\":true}");
            shouldRestart = true;
        }
    );

    // Energia acumulada
    server.on("/api/energy", HTTP_GET, [](AsyncWebServerRequest* req) {
        char buf[32];
        snprintf(buf, sizeof(buf), "{\"kwh\":%.3f}", energyKwh);
        req->send(200, "application/json", buf);
    });

    // Reset do acumulador de energia
    server.on("/api/reset-energy", HTTP_POST, [](AsyncWebServerRequest* req) {
        energyKwh = 0.0f; saveEnergy();
        Serial.println("[Energy] Acumulador zerado");
        req->send(200, "application/json", "{\"ok\":true,\"kwh\":0.0}");
    });

    // Reinício remoto
    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"ok\":true}");
        shouldRestart = true;
    });

    // Captive portal
    server.onNotFound([](AsyncWebServerRequest* req) {
        if (LittleFS.exists("/index.html"))
            req->send(LittleFS, "/index.html", "text/html");
        else
            req->redirect("/");
    });

    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    server.begin();
    Serial.printf("[HTTP] Servidor em http://%s\n", WiFi.softAPIP().toString().c_str());
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n=== %s v%s ===\n", FIRMWARE_MODEL, FIRMWARE_VERSION);

    pinMode(DHT_PIN, INPUT_PULLUP);
    dht.begin();
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    setupFS();
    loadAPConfig();   // carrega SSID/senha personalizados antes do setupAP
    loadHistory();
    loadEnergy();
    setupAP();
    setupServer();
}

void loop() {
    static unsigned long lastRead = 0;

    dnsServer.processNextRequest();
    ws.cleanupClients();

    // Reinício agendado (após salvar config ou requisição /api/restart)
    if (shouldRestart) {
        Serial.println("[Sistema] Reiniciando em 1s...");
        delay(1000);
        ESP.restart();
    }

    if (millis() - lastRead >= READ_INTERVAL) {
        lastRead = millis();

        float temp     = dht.readTemperature();
        float humidity = dht.readHumidity();
        if (isnan(temp) || isnan(humidity)) {
            Serial.println("[DHT11] Falha na leitura — usando 0.0");
            temp = 0.0f; humidity = 0.0f;
        }

        float v1 = readRMS(VOLTAGE_PIN_L1, VOLTAGE_SCALE, VOLTAGE_NOISE_FLOOR);
        float v2 = readRMS(VOLTAGE_PIN_L2, VOLTAGE_SCALE, VOLTAGE_NOISE_FLOOR);
        float v3 = readRMS(VOLTAGE_PIN_L3, VOLTAGE_SCALE, VOLTAGE_NOISE_FLOOR);

        float i1 = readRMS(CURRENT_PIN_L1, CURRENT_SCALE, CURRENT_NOISE_FLOOR);
        float i2 = readRMS(CURRENT_PIN_L2, CURRENT_SCALE, CURRENT_NOISE_FLOOR);
        float i3 = readRMS(CURRENT_PIN_L3, CURRENT_SCALE, CURRENT_NOISE_FLOOR);

        float p1 = (v1*i1)/1000.0f, p2 = (v2*i2)/1000.0f, p3 = (v3*i3)/1000.0f;
        float pTotal = p1 + p2 + p3;

        isGenerating = (pTotal > 0.5f);
        energyKwh   += pTotal * (READ_INTERVAL / 3600000.0f);

        Reading r;
        r.v1=v1; r.v2=v2; r.v3=v3;
        r.i1=i1; r.i2=i2; r.i3=i3;
        r.p1=p1; r.p2=p2; r.p3=p3;
        r.pTotal=pTotal; r.temp=temp; r.humidity=humidity;
        r.ts = millis()/1000;

        addReading(r); saveReading(r); saveEnergy();
        broadcastReading(r); checkAlerts(r);

        Serial.printf("[Leitura] V:%.1f/%.1f/%.1f V  I:%.2f/%.2f/%.2f A  "
                      "P:%.2f kVA  T:%.1f°C H:%.1f%%  E:%.3f kWh  Gen:%s\n",
                      v1,v2,v3, i1,i2,i3, pTotal, temp,humidity,
                      energyKwh, isGenerating?"SIM":"NAO");
    }
}
