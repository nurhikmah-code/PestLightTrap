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
#define LIGHT_SENSOR 14
#define PROX_SENSOR 13
#define RELAY 12

// =====================================================
// FIREBASE URL (Diperbarui sesuai struktur baru)
// =====================================================
// Tembak langsung ke Root menggunakan metode PATCH
String firebaseURL = "https://"
                     "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                     "firebasedatabase.app/.json";

// Status tetap di dalam smart_pest_trap
String statusURL = "https://"
                   "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                   "firebasedatabase.app/smart_pest_trap/status.json";

// Auto Mode dipindah ke Root
String autoModeURL = "https://"
                     "smart-pest-trap-81a41-default-rtdb.asia-southeast1."
                     "firebasedatabase.app/auto_mode.json";

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

// STATUS VARIABEL
bool firebaseStatus = false;
bool autoMode = false;
bool wadahPenuh = false;
bool objectDetected = false;
bool alatAktif = false;
unsigned long objectStartTime = 0;
String cahayaStatus;
String relayStatus;

WebServer server(80);

void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera Error");
    return;
  }
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: image/jpeg");
  client.println("Connection: close");
  client.println();
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// BACA FIREBASE
void bacaFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // BACA STATUS
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

    // BACA AUTO MODE
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

  // PIN MODE
  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(PROX_SENSOR, INPUT);
  pinMode(RELAY, OUTPUT);

  // relay OFF awal
  digitalWrite(RELAY, HIGH);

  // WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("CONNECTING WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WIFI CONNECTED");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("WEB SERVER STARTED");

  // CAMERA CONFIG
  camera_config_t config;
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

  // SD CARD
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD CARD FAILED");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("NO SD CARD");
    return;
  }

  Serial.println("SD CARD OK");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  server.handleClient();
  bacaFirebase();

  // SENSOR CAHAYA
  int lightValue = analogRead(LIGHT_SENSOR);
  if (lightValue == HIGH) {
    cahayaStatus = "TERANG";
  } else {
    cahayaStatus = "GELAP";
  }

  // SENSOR PROXIMITY
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

  // MODE OTOMATIS vs MANUAL
  if (autoMode) {
    alatAktif = (lightValue == LOW && !wadahPenuh);
  } else {
    alatAktif = firebaseStatus;
  }

  // RELAY
  if (alatAktif) {
    digitalWrite(RELAY, LOW);
    relayStatus = "ON";
  } else {
    digitalWrite(RELAY, HIGH);
    relayStatus = "OFF";
  }

  // FOTO
  if (millis() - lastCapture >= captureInterval) {
    lastCapture = millis();
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      String path = "/photo" + String(photoNumber) + ".jpg";
      File file = SD_MMC.open(path.c_str(), FILE_WRITE);
      if (file) {
        file.write(fb->buf, fb->len);
        file.close();
        lastPhoto = path;
        photoNumber++;
      }
      esp_camera_fb_return(fb);
    }
  }

  // PREPARASI DATA SENSOR
  int trapPercent = wadahPenuh ? 100 : 50;
  int batteryPercent = 100;
  int powerConsumption = 80;

  // =====================================================
  // JSON FIREBASE (Disusun ulang sesuai JSON Aplikasi Baru)
  // =====================================================

  String jsonData = "{";

  // 1. smart_pest_trap
  jsonData += "\"smart_pest_trap\":{";
  jsonData += "\"status\":" + String(firebaseStatus ? "true" : "false");
  jsonData += "},";

  // 2. operation_mode
  jsonData += "\"operation_mode\":{";
  jsonData += "\"days\":\"0,1,2,3,4,5,6,\",";
  jsonData += "\"start_time\":\"18:00\",";
  jsonData += "\"end_time\":\"06:00\"";
  jsonData += "},";

  // 3. auto_mode
  jsonData += "\"auto_mode\":" + String(autoMode ? "true" : "false") + ",";

  // 4. trap_fullness
  jsonData += "\"trap_fullness\":" + String(trapPercent) + ",";

  // 5. power_consumption
  jsonData += "\"power_consumption\":" + String(powerConsumption) + ",";

  // 6. battery
  jsonData += "\"battery\":{";
  jsonData += "\"percent\":" + String(batteryPercent) + ",";
  jsonData += "\"voltage\":\"12.6V\",";
  jsonData += "\"health\":\"BAIK\"";
  jsonData += "},";

  // 7. trap_analysis
  jsonData += "\"trap_analysis\":{";
  jsonData += "\"durasi_siklus\":\"SIKLUS MALAM\",";
  jsonData += "\"mulai\":\"18:00\",";
  jsonData += "\"selesai\":\"06:00\",";
  jsonData += "\"durasi\":\"12 JAM\"";
  jsonData += "}";

  jsonData += "}";

  // =====================================================
  // SEND FIREBASE (Menggunakan PATCH)
  // =====================================================

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(firebaseURL);
    http.addHeader("Content-Type", "application/json");

    // Menggunakan PATCH agar tidak menimpa data lain di Root
    int httpResponseCode = http.sendRequest("PATCH", jsonData);

    Serial.print("FIREBASE SYNC CODE : ");
    Serial.println(httpResponseCode);

    http.end();
  }

  // SERIAL LOGGING
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
  Serial.println("======================");

  delay(3000);
}