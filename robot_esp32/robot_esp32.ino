// Библиотеки (Library Manager):
//   "ArduinoJson" by Benoit Blanchon
//   "Adafruit NeoPixel"
//
// Протокол: WebSocket (RFC 6455) порт 8080
//   Mac → ESP32:
//     Tank:  {"type":"tank",  "left":<-100..100>,"right":<-100..100>,"light":<bool>,"extra":<bool>}
//     Steer: {"type":"steer","throttle":<-100..100>,"angle":<-100..100>,"light":<bool>,"extra":<bool>}
//   ESP32 → Mac (раз в BATTERY_SEND_INTERVAL мс):
//     {"type":"battery","voltage":11.8,"percent":72}
//
// ВАЖНО: впиши IP твоего Мака из приложения в CONTROLLER_IP ниже.
// Запусти приложение → нажми Connect → скопируй IP (без порта).

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

const char* SSID            = "a";
const char* PASSWORD        = "a";
const char* CONTROLLER_IP   = "a";  // ← IP из приложения
const int   CONTROLLER_PORT = 8080;

#define AIN1 13
#define AIN2 14
#define PWMA 26
#define BIN1 12
#define BIN2 27
#define PWMB 25
#define STBY 33

#define RELAY_PIN  15
#define NEO_PIN    16
#define NEO_COUNT  16

// Debug-светодиод: загорается при получении любой команды, гаснет если
// команды не приходили дольше DEBUG_LED_TIMEOUT мс (используется пока нет моторов)
#define DEBUG_LED          2
#define DEBUG_LED_TIMEOUT  300

unsigned long lastCmdAt   = 0;
int  lastLeftDbg          = 0;
int  lastRightDbg         = 0;
int  lastThrottleDbg      = 0;
int  lastAngleDbg         = 0;
bool lastLightDbg         = false;
bool lastExtraDbg         = false;

// ── Батарея ───────────────────────────────────────────────────────────────────
#define BATTERY_PIN             34     // ADC1_CH6, input-only, безопасно с WiFi
#define BATTERY_SEND_INTERVAL   2000   // мс

unsigned long lastBatterySend = 0;

Adafruit_NeoPixel pixels(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

WiFiClient     tcp;
bool           wsConnected       = false;
bool           robotConnected    = false;
unsigned long  lastConnectAttempt = 0;

bool lightOn   = false;
bool extraOn   = false;
bool lastLight = false;

// ── Моторы ────────────────────────────────────────────────────────────────────
void setupMotors() {
    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
    pinMode(STBY, OUTPUT);
    digitalWrite(STBY, HIGH);
    ledcAttach(PWMA, 5000, 8);
    ledcAttach(PWMB, 5000, 8);
}

void driveMotor1(int speed) {
    if      (speed > 0) { digitalWrite(AIN1,HIGH); digitalWrite(AIN2,LOW);  ledcWrite(PWMA, speed);  }
    else if (speed < 0) { digitalWrite(AIN1,LOW);  digitalWrite(AIN2,HIGH); ledcWrite(PWMA, -speed); }
    else                { digitalWrite(AIN1,LOW);  digitalWrite(AIN2,LOW);  ledcWrite(PWMA, 0);      }
}

void driveMotor2(int speed) {
    if      (speed > 0) { digitalWrite(BIN1,HIGH); digitalWrite(BIN2,LOW);  ledcWrite(PWMB, speed);  }
    else if (speed < 0) { digitalWrite(BIN1,LOW);  digitalWrite(BIN2,HIGH); ledcWrite(PWMB, -speed); }
    else                { digitalWrite(BIN1,LOW);  digitalWrite(BIN2,LOW);  ledcWrite(PWMB, 0);      }
}

void stopMotors() { driveMotor1(0); driveMotor2(0); }

static int toMotorSpeed(int val) {
    return constrain(map(val, -100, 100, -255, 255), -255, 255);
}

// ── Обработка JSON ────────────────────────────────────────────────────────────
void handleMessage(const String &msg) {
    JsonDocument doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    const char* type = doc["type"];
    if (!type) return;

    if (doc.containsKey("light")) lightOn = doc["light"].as<bool>();
    if (doc.containsKey("extra")) {
        extraOn = doc["extra"].as<bool>();
        digitalWrite(RELAY_PIN, extraOn ? HIGH : LOW);
    }

    int left = 0, right = 0, thr = 0, ang = 0;
    if (strcmp(type, "tank") == 0) {
        left  = doc["left"]  | 0;
        right = doc["right"] | 0;
        driveMotor1(toMotorSpeed(left));
        driveMotor2(toMotorSpeed(right));
    } else if (strcmp(type, "steer") == 0) {
        thr = doc["throttle"] | 0;
        ang = doc["angle"]    | 0;
        driveMotor1(toMotorSpeed(constrain(thr + ang, -100, 100)));
        driveMotor2(toMotorSpeed(constrain(thr - ang, -100, 100)));
    }

    // Debug: лог + светодиод только если что-то реально изменилось
    bool changed = (left != lastLeftDbg) || (right != lastRightDbg) ||
                   (thr  != lastThrottleDbg) || (ang != lastAngleDbg) ||
                   (lightOn != lastLightDbg) || (extraOn != lastExtraDbg);

    if (changed) {
        lastLeftDbg = left; lastRightDbg = right;
        lastThrottleDbg = thr; lastAngleDbg = ang;
        lastLightDbg = lightOn; lastExtraDbg = extraOn;

        lastCmdAt = millis();
        digitalWrite(DEBUG_LED, HIGH);

        if (strcmp(type, "tank") == 0)
            Serial.printf("[TANK] left=%d right=%d light=%s extra=%s\n",
                           left, right, lightOn ? "ON" : "off", extraOn ? "ON" : "off");
        else
            Serial.printf("[STEER] throttle=%d angle=%d light=%s extra=%s\n",
                           thr, ang, lightOn ? "ON" : "off", extraOn ? "ON" : "off");
    }
}

// ── Чтение точного числа байт с таймаутом ────────────────────────────────────
bool tcpReadBytes(uint8_t *buf, size_t n, unsigned long timeoutMs = 2000) {
    size_t got = 0;
    unsigned long start = millis();
    while (got < n) {
        if (!tcp.connected()) return false;
        if (tcp.available()) {
            buf[got++] = (uint8_t)tcp.read();
        } else if (millis() - start > timeoutMs) {
            return false;
        }
    }
    return true;
}

// ── WebSocket: отправить pong (ответ на ping) ─────────────────────────────────
void wsSendPong(uint8_t *payload, size_t len) {
    // Клиент обязан маскировать кадры (RFC 6455 §5.3)
    const uint8_t mask[4] = {0x37, 0xFA, 0x21, 0x3D};
    tcp.write((uint8_t)0x8A);                        // FIN + opcode=pong
    tcp.write((uint8_t)(0x80 | (uint8_t)len));       // MASK + len (<126)
    tcp.write(mask, 4);
    for (size_t i = 0; i < len; i++)
        tcp.write((uint8_t)(payload[i] ^ mask[i % 4]));
}

// ── WebSocket: отправить текстовый кадр (ESP32 → Mac) ─────────────────────────
void wsSendText(const String &text) {
    if (!wsConnected) return;
    size_t len = text.length();
    if (len > 65535) return;  // не ожидается для наших сообщений

    const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    tcp.write((uint8_t)0x81);  // FIN + opcode=text

    if (len <= 125) {
        tcp.write((uint8_t)(0x80 | len));
    } else {
        tcp.write((uint8_t)(0x80 | 126));
        tcp.write((uint8_t)(len >> 8));
        tcp.write((uint8_t)(len & 0xFF));
    }
    tcp.write(mask, 4);
    for (size_t i = 0; i < len; i++)
        tcp.write((uint8_t)((uint8_t)text[i] ^ mask[i % 4]));
}

// ── Батарея ───────────────────────────────────────────────────────────────────
float readBatteryVoltage() {
    int raw = analogRead(BATTERY_PIN);   // 0..4095
    return (raw / 4095.0f) * 3.3f;       // напряжение на пине ESP32
}

int batteryPercent(float voltage) {
    float pct = (voltage / 3.3f) * 100.0f;
    return constrain((int)pct, 0, 100);
}

void sendBatteryStatus() {
    float vBat = readBatteryVoltage();
    int   pct  = batteryPercent(vBat);

    char buf[80];
    snprintf(buf, sizeof(buf), "{\"type\":\"battery\",\"voltage\":%.2f,\"percent\":%d}", vBat, pct);
    wsSendText(String(buf));

    Serial.printf("[BAT] %.2fV (%d%%)\n", vBat, pct);
}

// ── WebSocket: прочитать один входящий кадр ───────────────────────────────────
// Возвращает true если получен текстовый кадр (заполняет outMsg).
// Автоматически отвечает на ping и обрабатывает close.
bool wsReadFrame(String &outMsg) {
    if (tcp.available() < 2) return false;

    uint8_t hdr[2];
    if (!tcpReadBytes(hdr, 2)) return false;

    uint8_t opcode     = hdr[0] & 0x0F;
    bool    masked     = (hdr[1] & 0x80) != 0;
    size_t  payloadLen = hdr[1] & 0x7F;

    if (payloadLen == 126) {
        uint8_t b[2];
        if (!tcpReadBytes(b, 2)) return false;
        payloadLen = ((uint16_t)b[0] << 8) | b[1];
    } else if (payloadLen == 127) {
        // Сообщение >65535 байт — в нашем случае невозможно
        uint8_t b[8];
        if (!tcpReadBytes(b, 8)) return false;
        payloadLen = 0;
    }

    if (payloadLen > 2048) { wsConnected = false; return false; }

    uint8_t maskKey[4] = {0, 0, 0, 0};
    if (masked) {
        if (!tcpReadBytes(maskKey, 4)) return false;
    }

    uint8_t *payload = (uint8_t*)malloc(payloadLen + 1);
    if (!payload) return false;

    if (!tcpReadBytes(payload, payloadLen)) { free(payload); return false; }

    if (masked) {
        for (size_t i = 0; i < payloadLen; i++) payload[i] ^= maskKey[i % 4];
    }
    payload[payloadLen] = '\0';

    bool result = false;
    switch (opcode) {
        case 0x01:  // text frame
        case 0x00:  // continuation frame
            outMsg = String((char*)payload);
            result = true;
            break;
        case 0x09:  // ping → pong
            wsSendPong(payload, payloadLen);
            break;
        case 0x08:  // close
            wsConnected = false;
            break;
        // 0x0A = pong, игнорируем
    }

    free(payload);
    return result;
}

// ── WebSocket: HTTP-рукопожатие ───────────────────────────────────────────────
bool wsHandshake() {
    tcp.print("GET / HTTP/1.1\r\n");
    tcp.print("Host: ");
    tcp.print(CONTROLLER_IP);
    tcp.print(":");
    tcp.print(CONTROLLER_PORT);
    tcp.print("\r\n");
    tcp.print("Upgrade: websocket\r\n");
    tcp.print("Connection: Upgrade\r\n");
    tcp.print("Sec-WebSocket-Key: cGhvdG9uaWNyb2JvdDEyMzQ=\r\n");
    tcp.print("Sec-WebSocket-Version: 13\r\n");
    tcp.print("\r\n");

    // Читаем заголовки до пустой строки
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 4000) {
        if (tcp.available()) {
            char c = (char)tcp.read();
            response += c;
            if (response.endsWith("\r\n\r\n")) break;
        }
    }
    Serial.println("[WS] ответ сервера: " + response.substring(0, response.indexOf('\r')));
    return response.indexOf("101") != -1;
}

// ── Setup / Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    pinMode(DEBUG_LED, OUTPUT);
    digitalWrite(DEBUG_LED, LOW);

    pixels.begin(); pixels.clear(); pixels.show();
    setupMotors();

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    Serial.print("WiFi");
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
        Serial.print('.'); delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nОШИБКА WiFi");
        while (true) delay(1000);
    }
    Serial.printf("\nIP платы: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Контроллер: %s:%d\n", CONTROLLER_IP, CONTROLLER_PORT);
}

void loop() {
    if (!tcp.connected() || !wsConnected) {
        if (robotConnected) {
            Serial.println("[WS] соединение потеряно");
            stopMotors();
            lightOn = false; extraOn = false; lastLight = false;
            digitalWrite(RELAY_PIN, LOW);
            robotConnected = false;
        }
        if (tcp.connected()) tcp.stop();
        wsConnected = false;

        unsigned long now = millis();
        if (now - lastConnectAttempt < 3000) return;
        lastConnectAttempt = now;

        Serial.printf("[WS] подключаюсь к %s:%d...\n", CONTROLLER_IP, CONTROLLER_PORT);
        if (!tcp.connect(CONTROLLER_IP, CONTROLLER_PORT)) {
            Serial.println("[WS] TCP не удалось");
            return;
        }
        if (!wsHandshake()) {
            Serial.println("[WS] рукопожатие не удалось");
            tcp.stop();
            return;
        }
        wsConnected    = true;
        robotConnected = true;
        Serial.println("[WS] подключено!");
        return;
    }

    // Читать входящие кадры
    String msg;
    if (wsReadFrame(msg)) handleMessage(msg);

    // Периодически слать уровень заряда батареи
    if (millis() - lastBatterySend >= BATTERY_SEND_INTERVAL) {
        lastBatterySend = millis();
        sendBatteryStatus();
    }

    // Гасим debug-светодиод если давно не было новых команд
    if (millis() - lastCmdAt > DEBUG_LED_TIMEOUT)
        digitalWrite(DEBUG_LED, LOW);

    // Обновить светодиоды при изменении
    if (lightOn != lastLight) {
        lastLight = lightOn;
        if (lightOn)
            for (int i = 0; i < NEO_COUNT; i++)
                pixels.setPixelColor(i, pixels.Color(255, 255, 255));
        else
            pixels.clear();
        pixels.show();
    }
}
