// Библиотеки (Library Manager):
//   "ArduinoJson" by Benoit Blanchon
//   "Adafruit NeoPixel"
//
// Протокол: newline-delimited JSON по TCP порт 8080
//   Tank:  {"type":"tank",  "left":<-100..100>,"right":<-100..100>,"light":<bool>,"extra":<bool>}
//   Steer: {"type":"steer","throttle":<-100..100>,"angle":<-100..100>,"light":<bool>,"extra":<bool>}
//
// ВАЖНО: впиши IP твоего Мака из приложения в CONTROLLER_IP ниже.
// Запусти приложение → нажми Connect → скопируй IP (без порта).

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

const char* SSID          = "2-408";
const char* PASSWORD      = "fefm-is-the-goat";
const char* CONTROLLER_IP = "192.168.0.102";  // ← IP из приложения
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

Adafruit_NeoPixel pixels(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

WiFiClient client;
String     tcpBuffer = "";
unsigned long lastConnectAttempt = 0;

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

// ── Обработка JSON-сообщения ─────────────────────────────────────────────────
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

    if (strcmp(type, "tank") == 0) {
        driveMotor1(toMotorSpeed(doc["left"]  | 0));
        driveMotor2(toMotorSpeed(doc["right"] | 0));
    }
    else if (strcmp(type, "steer") == 0) {
        int thr = doc["throttle"] | 0;
        int ang = doc["angle"]    | 0;
        driveMotor1(toMotorSpeed(constrain(thr + ang, -100, 100)));
        driveMotor2(toMotorSpeed(constrain(thr - ang, -100, 100)));
    }
}

// ── Setup / Loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

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
    Serial.printf("Подключаюсь к контроллеру %s:%d\n", CONTROLLER_IP, CONTROLLER_PORT);
}

void loop() {
    // Переподключение каждые 3 секунды если нет соединения
    if (!client.connected()) {
        if (client) {
            Serial.println("[TCP] отключился от контроллера");
            stopMotors();
            lightOn = false; extraOn = false;
            digitalWrite(RELAY_PIN, LOW);
            tcpBuffer = "";
        }

        unsigned long now = millis();
        if (now - lastConnectAttempt >= 3000) {
            lastConnectAttempt = now;
            Serial.printf("[TCP] подключаюсь к %s:%d...\n", CONTROLLER_IP, CONTROLLER_PORT);
            if (client.connect(CONTROLLER_IP, CONTROLLER_PORT)) {
                Serial.println("[TCP] подключено!");
            } else {
                Serial.println("[TCP] не удалось подключиться");
            }
        }
        return;
    }

    // Читать данные построчно
    while (client.available()) {
        char c = (char)client.read();
        if (c == '\n') {
            tcpBuffer.trim();
            if (tcpBuffer.length() > 0) handleMessage(tcpBuffer);
            tcpBuffer = "";
        } else {
            tcpBuffer += c;
        }
    }

    // Обновить светодиоды только при изменении
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
