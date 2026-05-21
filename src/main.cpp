#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"

// =====================================================
// WIFI
// =====================================================

#define WIFI_SSID "manes"
#define WIFI_PASSWORD "Kalem900"

// =====================================================
// SENSOR & RELAY
// =====================================================

#define LIGHT_SENSOR 14

#define PROX_SENSOR 13

#define RELAY 12

// =====================================================
// FIREBASE
// =====================================================

String firebaseURL =
"https://smart-pest-trap-81a41-default-rtdb.asia-southeast1.firebasedatabase.app/smart_pest_trap/status.json";

// =====================================================
// CAMERA PIN AI THINKER
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =====================================================
// TIMER CAMERA
// =====================================================

// 1 MENIT
unsigned long lastCapture = 0;

const unsigned long captureInterval =
60000;

// =====================================================
// FOTO
// =====================================================

int photoNumber = 0;

String lastPhoto = "none";

// =====================================================
// TIMER PROXIMITY
// =====================================================

unsigned long objectStartTime = 0;

bool objectDetected = false;

bool wadahPenuh = false;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();

  Serial.println("SMART PEST TRAP START");

  // =====================================================
  // PIN MODE
  // =====================================================

  pinMode(LIGHT_SENSOR, INPUT);

  pinMode(PROX_SENSOR, INPUT);

  pinMode(RELAY, OUTPUT);

  // relay OFF awal
  digitalWrite(RELAY, HIGH);

  // =====================================================
  // WIFI CONNECT
  // =====================================================

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("WIFI CONNECTED");

  Serial.print("IP : ");

  Serial.println(WiFi.localIP());

  // =====================================================
  // CAMERA CONFIG
  // =====================================================

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

  // ukuran ringan & stabil
  config.frame_size = FRAMESIZE_QVGA;

  // kualitas sedang
  // makin besar angka = makin kecil size
  config.jpeg_quality = 15;

  config.fb_count = 1;

  esp_err_t err =
  esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf(
      "Camera init failed: 0x%x\n",
      err
    );

    return;
  }

  Serial.println("CAMERA OK");

  // =====================================================
  // SD CARD
  // =====================================================

  if(!SD_MMC.begin()){

    Serial.println("SD CARD FAILED");

    return;
  }

  Serial.println("SD CARD OK");

  Serial.println("======================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // =====================================================
  // READ SENSOR
  // =====================================================

  int lightValue =
  digitalRead(LIGHT_SENSOR);

  int proxValue =
  digitalRead(PROX_SENSOR);

  String cahayaStatus;

  String relayStatus;

  String wadahStatus;

  // =====================================================
  // SENSOR CAHAYA
  // =====================================================

  if(lightValue == HIGH){

    cahayaStatus = "GELAP";
  }
  else{

    cahayaStatus = "TERANG";
  }

  // =====================================================
  // SENSOR PROXIMITY
  // =====================================================

  if(proxValue == LOW){

    // pertama kali deteksi
    if(!objectDetected){

      objectDetected = true;

      objectStartTime = millis();

      Serial.println("OBJECT DETECTED");
    }

    // jika objek diam 1 menit
    if(millis() - objectStartTime >= 60000){

      wadahPenuh = true;
    }
  }
  else{

    // reset jika objek hilang
    objectDetected = false;

    wadahPenuh = false;
  }

  // =====================================================
  // STATUS WADAH
  // =====================================================

  if(wadahPenuh){

    wadahStatus = "PENUH";
  }
  else{

    wadahStatus = "BELUM PENUH";
  }

  // =====================================================
  // RELAY
  // =====================================================

  // relay ON jika:
  // GELAP + WADAH BELUM PENUH

  if(lightValue == HIGH && !wadahPenuh){

    digitalWrite(RELAY, LOW);

    relayStatus = "ON";
  }
  else{

    digitalWrite(RELAY, HIGH);

    relayStatus = "OFF";
  }

  // =====================================================
  // SERIAL MONITOR
  // =====================================================

  Serial.print("CAHAYA : ");

  Serial.println(cahayaStatus);

  Serial.print("WADAH  : ");

  Serial.println(wadahStatus);

  Serial.print("RELAY  : ");

  Serial.println(relayStatus);

  // =====================================================
  // CAPTURE FOTO TIAP 1 MENIT
  // =====================================================

  if(millis() - lastCapture >= captureInterval){

    lastCapture = millis();

    Serial.println("CAPTURING PHOTO...");

    camera_fb_t * fb =
    esp_camera_fb_get();

    if(!fb){

      Serial.println("CAPTURE FAILED");
    }
    else{

      String path =
      "/photo" +
      String(photoNumber) +
      ".jpg";

      File file =
      SD_MMC.open(path.c_str(), FILE_WRITE);

      if(!file){

        Serial.println("FILE FAILED");
      }
      else{

        file.write(fb->buf, fb->len);

        file.close();

        Serial.println("PHOTO SAVED");

        Serial.println(path);

        lastPhoto = path;

        photoNumber++;
      }

      esp_camera_fb_return(fb);
    }
  }

  // =====================================================
  // JSON FIREBASE
  // =====================================================

  String jsonData = "{";

  jsonData += "\"cahaya\":\"" + cahayaStatus + "\",";

  jsonData += "\"wadah\":\"" + wadahStatus + "\",";

  jsonData += "\"relay\":\"" + relayStatus + "\",";

  jsonData += "\"kamera\":\"AKTIF\",";

  jsonData += "\"foto_terakhir\":\"" + lastPhoto + "\"";

  jsonData += "}";

  Serial.println(jsonData);

  // =====================================================
  // SEND FIREBASE
  // =====================================================

  if(WiFi.status() == WL_CONNECTED){

    HTTPClient http;

    http.begin(firebaseURL);

    http.addHeader(
      "Content-Type",
      "application/json"
    );

    int httpResponseCode =
    http.PUT(jsonData);

    if(httpResponseCode > 0){

      Serial.print("SEND SUCCESS : ");

      Serial.println(httpResponseCode);
    }
    else{

      Serial.print("SEND FAILED : ");

      Serial.println(httpResponseCode);
    }

    http.end();
  }

  Serial.println("======================");

  delay(3000);
}