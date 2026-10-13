#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_TMP117.h>
#include <7Semi_BMI330.h>
#include <time.h>
#include "secrets.h"
// =====================================================
// HARDWARE PIN CONFIGURATION
// =====================================================
// TMP117: separate I2C bus 0
#define TMP117_SDA       21
#define TMP117_SCL       22
#define TMP117_ADDRESS   0x49
// BMI330: separate I2C bus 1
#define BMI330_SDA       25
#define BMI330_SCL       26
#define BMI330_ADDRESS   0x69
// LED
#define LED_PIN          18
// Timing
const unsigned long FIREBASE_INTERVAL_MS = 10000;
const unsigned long WIFI_RETRY_MS = 10000;
// =====================================================
// SENSOR OBJECTS
// =====================================================
TwoWire TMPWire(0);
TwoWire BMIWire(1);
Adafruit_TMP117 tmp117;
BMI330_7Semi imu;
bool tmp117Ready = false;
bool bmi330Ready = false;
bool stepCounterReady = false;
bool firebaseReady = false;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastWiFiRetry = 0;
float temperatureC = NAN;
uint32_t stepCount = 0;
// Firebase authentication token
String firebaseIdToken = "";
String firebaseRefreshToken = "";
// =====================================================
// FIREBASE URL HELPERS
// =====================================================
String firebaseUrl(const String &path) {
    String url = String(FIREBASE_DATABASE_URL) + path;
    if (firebaseIdToken.length() > 0) {
        url += (path.indexOf('?') >= 0) ? "&auth=" : "?auth=";
        url += firebaseIdToken;
    }
    return url;
}
// =====================================================
// WIFI DIAGNOSTICS
// =====================================================
void printWiFiStatus() {
    Serial.printf("Wi-Fi status code: %d\n", (int)WiFi.status());
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal strength (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    }
}
bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 20000) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected successfully.");
        printWiFiStatus();
        return true;
    }
    Serial.println("Wi-Fi connection failed.");
    printWiFiStatus();
    Serial.println("Scanning nearby Wi-Fi networks...");
    int count = WiFi.scanNetworks();
    if (count <= 0) {
        Serial.println("No Wi-Fi networks found.");
    } else {
        for (int i = 0; i < count; i++) {
            Serial.printf(
                "%s | RSSI: %d dBm | Channel: %d | Security: %d\n",
                WiFi.SSID(i).c_str(),
                WiFi.RSSI(i),
                WiFi.channel(i),
                (int)WiFi.encryptionType(i)
            );
        }
    }
    WiFi.scanDelete();
    return false;
}
// =====================================================
// FIREBASE EMAIL/PASSWORD AUTHENTICATION
// =====================================================
bool signInFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot authenticate: Wi-Fi disconnected.");
        return false;
    }
    if (String(FIREBASE_API_KEY) == "YOUR_FIREBASE_WEB_API_KEY" ||
        String(FIREBASE_EMAIL) == "YOUR_FIREBASE_TEST_USER_EMAIL" ||
        String(FIREBASE_PASSWORD) == "YOUR_FIREBASE_TEST_USER_PASSWORD") {
        Serial.println("ERROR: Set real Firebase credentials in secrets.h.");
        return false;
    }
    WiFiClientSecure client;
    client.setInsecure(); // Testing only; use certificate validation in production.
    HTTPClient http;
    String url =
        "https://identitytoolkit.googleapis.com/v1/"
        "accounts:signInWithPassword?key=" +
        String(FIREBASE_API_KEY);
    if (!http.begin(client, url)) {
        Serial.println("Firebase authentication: HTTP begin failed.");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    JsonDocument request;
    request["email"] = FIREBASE_EMAIL;
    request["password"] = FIREBASE_PASSWORD;
    request["returnSecureToken"] = true;
    String body;
    serializeJson(request, body);
    int code = http.POST(body);
    String response = http.getString();
    http.end();
    if (code != 200) {
        Serial.printf("Firebase sign-in failed: HTTP %d\n", code);
        Serial.println(response);
        firebaseReady = false;
        return false;
    }
    JsonDocument result;
    DeserializationError error = deserializeJson(result, response);
    if (error || !result["idToken"].is<const char *>()) {
        Serial.println("Invalid Firebase authentication response.");
        return false;
    }
    firebaseIdToken = result["idToken"].as<String>();
    firebaseRefreshToken = result["refreshToken"].as<String>();
    firebaseReady = true;
    Serial.println("Firebase authentication successful.");
    return true;
}
// =====================================================
// FIREBASE REST REQUEST
// =====================================================
bool firebaseRequest(
    const String &path,
    const String &method,
    const String &payload,
    String *responseOut = nullptr
) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Firebase request skipped: Wi-Fi disconnected.");
        return false;
    }
    if (!firebaseReady || firebaseIdToken.isEmpty()) {
        if (!signInFirebase()) {
            return false;
        }
    }
    WiFiClientSecure client;
    client.setInsecure(); // Testing only; use certificate validation in production.
    HTTPClient http;
    String url = firebaseUrl(path);
    if (!http.begin(client, url)) {
        Serial.println("Firebase HTTP begin failed.");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    int code = -1;
    if (method == "GET") {
        code = http.GET();
    } else if (method == "PATCH") {
        code = http.sendRequest("PATCH", payload);
    } else if (method == "PUT") {
        code = http.PUT(payload);
    }
    String response = http.getString();
    http.end();
    if (responseOut != nullptr) {
        *responseOut = response;
    }
    if (code >= 200 && code < 300) {
        Serial.printf("Firebase %s successful: %s (HTTP %d)\n",
                      method.c_str(), path.c_str(), code);
        return true;
    }
    Serial.printf("Firebase %s failed: HTTP %d\n",
                  method.c_str(), code);
    Serial.println(response);
    // An expired or invalid token may need a fresh sign-in.
    if (code == 401 || code == 403) {
        firebaseReady = false;
        firebaseIdToken = "";
    }
    return false;
}
// =====================================================
// SENSOR INITIALIZATION
// =====================================================
void initializeTMP117() {
    Serial.println("\nInitializing TMP117...");
    TMPWire.begin(TMP117_SDA, TMP117_SCL, 100000);
    if (tmp117.begin(TMP117_ADDRESS, &TMPWire)) {
        tmp117Ready = true;
        Serial.println("TMP117 initialized at 0x49.");
    } else {
        tmp117Ready = false;
        Serial.println("ERROR: TMP117 initialization failed.");
    }
}
void initializeBMI330() {
    Serial.println("\nInitializing BMI330...");
    BMIWire.begin(BMI330_SDA, BMI330_SCL, 400000);
    if (imu.beginI2C(BMI330_ADDRESS, BMIWire, 400000)) {
        bmi330Ready = true;
        Serial.println("BMI330 initialized at 0x69.");
        if (imu.enableStepCounter(true)) {
            stepCounterReady = true;
            Serial.println("BMI330 step counter enabled.");
        } else {
            stepCounterReady = false;
            Serial.println("ERROR: BMI330 step counter setup failed.");
        }
    } else {
        bmi330Ready = false;
        stepCounterReady = false;
        Serial.println("ERROR: BMI330 initialization failed.");
    }
}
// =====================================================
// SENSOR READINGS
// =====================================================
void readSensors() {
    if (tmp117Ready) {
        // Read the TMP117 using the Adafruit Unified Sensor API.
        sensors_event_t temperatureEvent;
        if (tmp117.getEvent(&temperatureEvent)) {
            temperatureC = temperatureEvent.temperature;
        } else {
            temperatureC = NAN;
        }

        if (isfinite(temperatureC)) {
            Serial.printf("Temperature: %.2f C\n", temperatureC);
        } else {
            Serial.println("Temperature reading invalid.");
        }
    }
    if (stepCounterReady) {
        uint32_t currentSteps = 0;
        if (imu.readStepCount(currentSteps)) {
            stepCount = currentSteps;
            Serial.printf("BMI330 steps: %lu\n",
                          (unsigned long)stepCount);
        } else {
            Serial.println("BMI330 step count read failed.");
        }
    }
}
// =====================================================
// SEND SENSOR DATA TO FIREBASE
// =====================================================
void uploadSensorData() {
    if (!firebaseReady && !signInFirebase()) {
        Serial.println("Skipping upload: Firebase not authenticated.");
        return;
    }
    // Upload temperature to the configured temperature path.
    if (tmp117Ready && isfinite(temperatureC)) {
        JsonDocument temperatureData;
        temperatureData["temperatureC"] = temperatureC;
        temperatureData["timestamp"] = (unsigned long)millis();
        String payload;
        serializeJson(temperatureData, payload);
        firebaseRequest(
            FIREBASE_TEMPERATURE_PATH,
            "PUT",
            payload
        );
    }
    // Upload step count separately.
    if (stepCounterReady) {
        JsonDocument stepData;
        stepData["steps"] = stepCount;
        stepData["timestamp"] = (unsigned long)millis();
        String payload;
        serializeJson(stepData, payload);
        firebaseRequest(
            "/sensors/steps.json",
            "PUT",
            payload
        );
    }
    // Publish device status.
    JsonDocument status;
    status["online"] = true;
    status["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    status["tmp117Ready"] = tmp117Ready;
    status["bmi330Ready"] = bmi330Ready;
    status["stepCounterReady"] = stepCounterReady;
    status["temperatureC"] = isfinite(temperatureC)
                                  ? temperatureC
                                  : 0.0;
    status["steps"] = stepCount;
    String statusPayload;
    serializeJson(status, statusPayload);
    firebaseRequest(
        FIREBASE_STATUS_PATH,
        "PUT",
        statusPayload
    );
}
// =====================================================
// READ REMOTE LED COMMAND FROM FIREBASE
// =====================================================
void updateLEDFromFirebase() {
    String response;
    if (!firebaseRequest(
            FIREBASE_LED_PATH,
            "GET",
            "",
            &response)) {
        return;
    }
    response.trim();
    // Supports a Firebase boolean value: true or false.
    if (response == "true") {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Remote LED command: ON");
    } else if (response == "false") {
        digitalWrite(LED_PIN, LOW);
        Serial.println("Remote LED command: OFF");
    } else {
        // Also supports a JSON object such as {"led":true}.
        JsonDocument command;
        DeserializationError error = deserializeJson(command, response);
        if (!error && command["led"].is<bool>()) {
            bool ledOn = command["led"].as<bool>();
            digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
            Serial.printf("Remote LED command: %s\n",
                          ledOn ? "ON" : "OFF");
        } else {
            Serial.print("Unrecognized LED value: ");
            Serial.println(response);
        }
    }
}
// =====================================================
// I2C SCANNER
// =====================================================
void scanI2CBus(TwoWire &bus, const char *name) {
    Serial.printf("\nScanning %s...\n", name);
    int found = 0;
    for (uint8_t address = 1; address < 127; address++) {
        bus.beginTransmission(address);
        if (bus.endTransmission() == 0) {
            Serial.printf("I2C device found at 0x%02X\n", address);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("No I2C devices found.");
    }
}
// =====================================================
// SETUP
// =====================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==================================");
    Serial.println("ESP32 Sensor + Firebase Application");
    Serial.println("==================================");
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    // Initialize independent I2C buses.
    TMPWire.begin(TMP117_SDA, TMP117_SCL, 100000);
    BMIWire.begin(BMI330_SDA, BMI330_SCL, 400000);
    scanI2CBus(TMPWire, "TMP117 I2C bus (GPIO 21/22)");
    scanI2CBus(BMIWire, "BMI330 I2C bus (GPIO 25/26)");
    initializeTMP117();
    initializeBMI330();
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        signInFirebase();
    }
    Serial.println("\nSetup complete.");
}
// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
    // Attempt Wi-Fi reconnection periodically.
    if (WiFi.status() != WL_CONNECTED &&
        millis() - lastWiFiRetry >= WIFI_RETRY_MS) {
        lastWiFiRetry = millis();
        connectWiFi();
    }
    // Read sensors frequently.
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead >= 1000) {
        lastSensorRead = millis();
        readSensors();
    }
    // Upload sensor data and check the LED command every 10 seconds.
    if (millis() - lastFirebaseUpdate >= FIREBASE_INTERVAL_MS) {
        lastFirebaseUpdate = millis();
        if (WiFi.status() == WL_CONNECTED) {
            if (!firebaseReady) {
                signInFirebase();
            }
            if (firebaseReady) {
                uploadSensorData();
                updateLEDFromFirebase();
            }
        } else {
            Serial.println("Skipping Firebase update: Wi-Fi offline.");
        }
    }
    delay(10);
}
