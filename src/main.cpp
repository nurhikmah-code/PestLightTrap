#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

// WIFI

#define WIFI_SSID "Ramadhani"
#define WIFI_PASSWORD "lemkaca2"

// SENSOR & RELAY
// SD_MMC mode 1-bit: hanya pakai GPIO 2 (DATA0), 14 (CLK), 15 (CMD)
// GPIO 4, 12, 13 bebas digunakan untuk sensor/relay

#define LIGHT_SENSOR 3 // Menggunakan pin U0R (RX)
#define PROX_SENSOR 13
#define RELAY 12
#define FLASH_LED 4 // Agar Flash LED bisa dimatikan permanen

// Catatan: LDR terhubung ke pin DO (Digital Output)
// Atur sensitivitas lewat potensio di modul LDR

// FIREBASE URL

String firebaseURL = "https://"
                     "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                     "firebasedatabase.app/smart_pest_trap.json";

String statusURL = "https://"
                   "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                   "firebasedatabase.app/smart_pest_trap/status.json";

String autoModeURL = "https://"
                     "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                     "firebasedatabase.app/smart_pest_trap/auto_mode.json";

// CAMERA PIN AI THINKER

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5

#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// TIMER CAMERA

unsigned long lastCapture = 0;

const unsigned long captureInterval = 60000;

// FOTO

int photoNumber = 0;

String lastPhoto = "none";

// STATUS

bool firebaseStatus = false;

bool autoMode = false;

bool wadahPenuh = false;

bool objectDetected = false;

bool alatAktif = false;

unsigned long objectStartTime = 0;

unsigned long lastFirebaseRead = 0;

const unsigned long firebaseInterval = 10000; // 10 detik

bool cameraOK = false;

// SIMULASI BATTERY
unsigned long lastBatteryUpdate = 0;
const unsigned long batteryUpdateInterval = 30000; // Update tiap 30 detik
float batteryLevel = 100.0;

String cahayaStatus;

String relayStatus;

WebServer server(80);

void handleCapture() {

  if (!cameraOK) {

    server.send(503, "text/plain", "Camera not initialized");

    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {

    server.send(500, "text/plain", "Camera Error");

    return;
  }

  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");

  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

// BACA FIREBASE

void bacaFirebase() {

  if (millis() - lastFirebaseRead < firebaseInterval) {
    return;
  }

  lastFirebaseRead = millis();

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    // STATUS

    http.begin(statusURL);

    int httpCode = http.GET();

    if (httpCode > 0) {

      String payload = http.getString();

      payload.trim();

      Serial.print("PAYLOAD STATUS : ");
      Serial.println(payload);

      if (payload == "true") {

        firebaseStatus = true;
      } else {

        firebaseStatus = false;
      }
    } else {

      Serial.println("FAILED GET STATUS");
    }

    http.end();

    // AUTO MODE

    http.begin(autoModeURL);

    httpCode = http.GET();

    if (httpCode > 0) {

      String payload = http.getString();

      payload.trim();

      Serial.print("PAYLOAD AUTO : ");
      Serial.println(payload);

      if (payload == "true") {

        autoMode = true;
      } else {

        autoMode = false;
      }
    } else {

      Serial.println("FAILED GET AUTO MODE");
    }

    http.end();
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();

  Serial.println("SMART PEST TRAP");

  // =====================================================
  // PIN MODE
  // =====================================================

  pinMode(LIGHT_SENSOR, INPUT);

  pinMode(PROX_SENSOR, INPUT);

  pinMode(RELAY, OUTPUT);
  pinMode(FLASH_LED, OUTPUT);

  // relay OFF awal, flash OFF awal
  digitalWrite(RELAY, HIGH);
  digitalWrite(FLASH_LED, LOW);

  // =====================================================
  // WIFI
  // =====================================================

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("CONNECTING WIFI");

  int wifiTimeout = 0;

  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 40) {

    delay(500);

    Serial.print(".");

    wifiTimeout++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WIFI CONNECTED");

    Serial.print("IP : ");

    Serial.println(WiFi.localIP());

    server.on("/capture", HTTP_GET, handleCapture);
    server.begin();

    Serial.println("WEB SERVER STARTED");
  } else {

    Serial.println("WIFI FAILED - continuing offline");
  }

  // =====================================================
  // CAMERA CONFIG
  // =====================================================

  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;

  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;

  config.pin_pclk = PCLK_GPIO_NUM;

  config.pin_vsync = VSYNC_GPIO_NUM;

  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;

  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;

  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA;

  config.jpeg_quality = 15;

  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.println("CAMERA FAILED");

    return;
  }

  Serial.println("CAMERA OK");

  cameraOK = true;

  // SD CARD

  if (!SD_MMC.begin("/sdcard",
                    true)) { // true = mode 1-bit (bebaskan GPIO 4, 12, 13)

    Serial.println("SD CARD FAILED");

    return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {

    Serial.println("NO SD CARD");

    return;
  }

  Serial.println("SD CARD OK");

  // Scan foto terakhir agar tidak menimpa file lama
  File root = SD_MMC.open("/");
  if (root) {
    File entry;
    while (entry = root.openNextFile()) {
      String name = String(entry.name());
      int photoIdx = name.indexOf("photo");
      if (photoIdx >= 0 && name.endsWith(".jpg")) {
        int numStart = photoIdx + 5;
        int numEnd = name.indexOf(".jpg");
        if (numStart < numEnd) {
          int num = name.substring(numStart, numEnd).toInt();
          if (num >= photoNumber) {
            photoNumber = num + 1;
          }
        }
      }
      entry.close();
    }
    root.close();
  }
  Serial.print("PHOTO NUMBER START: ");
  Serial.println(photoNumber);

  Serial.print("CARD TYPE : ");

  if (cardType == CARD_MMC) {

    Serial.println("MMC");
  } else if (cardType == CARD_SD) {

    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {

    Serial.println("SDHC");
  } else {

    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);

  Serial.print("CARD SIZE : ");
  Serial.print(cardSize);
  Serial.println(" MB");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  server.handleClient();

  // =====================================================
  // BACA FIREBASE
  // =====================================================

  bacaFirebase();

  // =====================================================
  // SENSOR CAHAYA
  // =====================================================

  // Pakai digitalRead karena LDR terhubung ke pin DO (Digital Output)
  int lightValue = digitalRead(LIGHT_SENSOR);

  Serial.print("LIGHT VALUE = ");
  Serial.println(lightValue);

  // DO modul LDR umumnya: LOW = terang, HIGH = gelap
  if (lightValue == LOW) {
    cahayaStatus = "TERANG";
  } else {
    cahayaStatus = "GELAP";
  }

  // =====================================================
  // SENSOR PROXIMITY
  // =====================================================

  int proxValue = digitalRead(PROX_SENSOR);

  if (proxValue == LOW) {

    if (!objectDetected) {

      objectDetected = true;

      objectStartTime = millis();

      Serial.println("OBJECT DETECTED");
    }

    if (millis() - objectStartTime >= 60000) {

      wadahPenuh = true;
    }
  } else {

    objectDetected = false;

    wadahPenuh = false;
  }

  // =====================================================
  // MODE OTOMATIS
  // =====================================================

  if (autoMode) {

    alatAktif =
        (lightValue == HIGH && !wadahPenuh); // HIGH = gelap = aktifkan perangkap
  }

  // =====================================================
  // MODE MANUAL
  // =====================================================

  else {

    alatAktif = firebaseStatus;
  }

  // =====================================================
  // RELAY
  // =====================================================

  if (alatAktif) {

    digitalWrite(RELAY, LOW);

    relayStatus = "ON";
  } else {

    digitalWrite(RELAY, HIGH);

    relayStatus = "OFF";
  }

  // =====================================================
  // FOTO
  // =====================================================

  if (cameraOK && millis() - lastCapture >= captureInterval) {

    lastCapture = millis();

    Serial.println("CAPTURING PHOTO");

    camera_fb_t *fb = esp_camera_fb_get();

    if (fb) {

      String path = "/esp32cam/photo" + String(photoNumber) + ".jpg";

      File file = SD_MMC.open(path.c_str(), FILE_WRITE);

      if (file) {

        file.write(fb->buf, fb->len);

        file.close();

        Serial.println("PHOTO SAVED");

        Serial.println(path);

        lastPhoto = path;

        photoNumber++;
      }

      esp_camera_fb_return(fb);
    } else {

      Serial.println("CAPTURE FAILED");
    }
  }

  // =====================================================
  // DATA SENSOR
  // =====================================================

  int trapPercent;

  if (wadahPenuh) {

    trapPercent = 100;
  } else if (objectDetected) {

    trapPercent = 50;
  } else {

    trapPercent = 0;
  }

  // =====================================================
  // SIMULASI BATTERY & POWER
  // =====================================================

  // Drain battery berdasarkan status alat (tiap 30 detik)
  if (millis() - lastBatteryUpdate >= batteryUpdateInterval) {
    lastBatteryUpdate = millis();

    float drainRate = 0.01; // Base drain: ESP32 + WiFi

    if (alatAktif) {
      drainRate += 0.04; // Relay ON (lampu UV perangkap)
    }
    if (cameraOK) {
      drainRate += 0.015; // Kamera aktif
    }

    // Fluktuasi acak ±20% agar terlihat natural
    float noise = 1.0 + (random(-20, 21) / 100.0);
    drainRate *= noise;

    batteryLevel -= drainRate;
    if (batteryLevel < 0.0)
      batteryLevel = 0.0;
  }

  int batteryPercent = (int)batteryLevel;

  // Simulasi konsumsi daya (Watt)
  float basePower = 2.5; // ESP32 + WiFi baseline

  if (alatAktif)
    basePower += 8.5; // Relay + lampu UV
  if (cameraOK)
    basePower += 1.5; // Kamera

  basePower += random(-50, 51) / 100.0; // Noise ±0.5W

  int powerConsumption = (int)basePower;
  if (powerConsumption < 1)
    powerConsumption = 1;

  // =====================================================
  // JSON FIREBASE
  // =====================================================

  String jsonData = "{";

  // =========================================
  // STATUS
  // =========================================

  jsonData += "\"status\":";

  jsonData += (firebaseStatus ? "true" : "false");

  jsonData += ",";

  // =========================================
  // AUTO MODE
  // =========================================

  jsonData += "\"auto_mode\":";

  jsonData += (autoMode ? "true" : "false");

  jsonData += ",";

  // =========================================
  // SENSOR
  // =========================================

  jsonData += "\"cahaya\":\"";

  jsonData += cahayaStatus;

  jsonData += "\",";

  jsonData += "\"relay\":\"";

  jsonData += relayStatus;

  jsonData += "\",";

  jsonData += "\"kamera\":\"";
  jsonData += (cameraOK ? "AKTIF" : "NONAKTIF");
  jsonData += "\",";

  jsonData += "\"foto_terakhir\":\"";

  jsonData += lastPhoto;

  jsonData += "\",";

  jsonData += "\"trap_fullness\":";

  jsonData += String(trapPercent);

  jsonData += ",";

  jsonData += "\"power_consumption\":";

  jsonData += String(powerConsumption);

  jsonData += ",";

  // =========================================
  // BATTERY
  // =========================================

  jsonData += "\"battery\":{";

  jsonData += "\"percent\":";

  jsonData += String(batteryPercent);

  jsonData += "},";

  // =========================================
  // OPERATION MODE
  // =========================================

  jsonData += "\"operation_mode\":{";

  jsonData += "\"days\":\"0,1,2,3,4\",";

  jsonData += "\"start_time\":\"18:00\",";

  jsonData += "\"end_time\":\"06:00\"";

  jsonData += "}";

  jsonData += "}";

  // =====================================================
  // SEND FIREBASE
  // =====================================================

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin(firebaseURL);

    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.PUT(jsonData);

    Serial.print("FIREBASE RESPONSE : ");

    Serial.println(httpResponseCode);

    http.end();
  }

  // =====================================================
  // SERIAL
  // =====================================================

  Serial.println("======================");

  Serial.print("AUTO MODE : ");

  Serial.println(autoMode);

  Serial.print("FIREBASE STATUS : ");

  Serial.println(firebaseStatus);

  Serial.print("ALAT AKTIF : ");

  Serial.println(alatAktif);

  Serial.print("RELAY : ");

  Serial.println(relayStatus);

  Serial.print("CAHAYA : ");

  Serial.println(cahayaStatus);

  Serial.print("BATTERY : ");
  Serial.print(batteryPercent);
  Serial.println("%");

  Serial.print("POWER : ");
  Serial.print(powerConsumption);
  Serial.println("W");

  Serial.println("======================");

  delay(3000);
}