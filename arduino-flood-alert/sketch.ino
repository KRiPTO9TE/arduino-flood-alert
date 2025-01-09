#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <driver/adc.h> // Untuk akses Hall Sensor melalui ADC

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* firebaseHost = "YOUR_FIREBASE_API";
const char* firebaseAuth = "YOUR_FIREBASE_API_KEY";
const char* ntpServer = "pool.ntp.org";
const int trigPin = 5;
const int echoPin = 18;

#define SOUND_SPEED 0.034
#define MEASUREMENT_INTERVAL 5000
#define MAX_RETRIES 3
#define MAX_TEMP_THRESHOLD 60 // Suhu maksimum yang aman (°C)
#define MAX_SENSOR_FAILURES 5 // Maksimum kegagalan pembacaan sensor       

String phoneNumber = "YOUR_PHONE_NUMBER"; 
String apiKey = "YOUR_CALLMEBOT_API_KEY";           

unsigned long lastMeasurementTime = 0;
int lastFloodStage = 0;
int sensorFailureCount = 0;

void resetDevice() {
   Serial.println("Malfungsi terdeteksi! Mereset perangkat...");
   ESP.restart(); // Restart alat
}

void setupWiFi() {
   int wifi_attempts = 0;
   WiFi.begin(ssid, password);
   Serial.print("Connecting to WiFi");
   while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
      wifi_attempts++;

      if (wifi_attempts >= MAX_RETRIES) {
         Serial.println("\nWiFi gagal tersambung setelah beberapa percobaan. Mereset perangkat...");
         resetDevice();
      }
   }
   Serial.println("\nWiFi Connected");
   Serial.println("IP Address: " + WiFi.localIP().toString());
}

void setupTime() {
   configTime(0, 0, ntpServer);
   Serial.print("Waiting for NTP time sync...");
   struct tm timeinfo;
   int time_attempts = 0;

   while (!getLocalTime(&timeinfo)) {
      Serial.print(".");
      delay(500);
      time_attempts++;

      if (time_attempts >= MAX_RETRIES) {
         Serial.println("\nGagal sinkronisasi waktu. Mereset perangkat...");
         resetDevice();
      }
   }
   Serial.println("\nTime synchronized");
}

unsigned long getEpochTime() {
   struct tm timeinfo;
   if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return 0;
   }
   return mktime(&timeinfo);
}

float measureDistance() {
   digitalWrite(trigPin, LOW);
   delayMicroseconds(2);
   digitalWrite(trigPin, HIGH);
   delayMicroseconds(10);
   digitalWrite(trigPin, LOW);

   long duration = pulseIn(echoPin, HIGH, 30000); // Timeout 30ms
   if (duration > 0) {
      return (duration * SOUND_SPEED / 2) / 100.0; // Distance in meters
   } else {
      return -1; // Indikasi kegagalan pembacaan
   }
}

void sendDataToFirebase(float distance) {
   if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String(firebaseHost) + "/water-level.json?auth=" + firebaseAuth;

      String jsonPayload = "{\"distance\":";
      jsonPayload += String((distance * 100));
      jsonPayload += ", \"unixTimestamp\":";
      jsonPayload += String(getEpochTime());
      jsonPayload += "}";

      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
         Serial.println("Data sent successfully");
      } else {
         Serial.println("Error sending data: " + String(httpResponseCode));
      }

      http.end();
   } else {
      Serial.println("WiFi not connected, data not sent");
   }
}

String urlEncode(String str) {
   String encodedString = "";
   char c;
   char code0;
   char code1;
   for (int i = 0; i < str.length(); i++) {
      c = str.charAt(i);
      if (isalnum(c)) {
         encodedString += c;
      } else {
         code1 = (c & 0xf) + '0';
         if ((c & 0xf) > 9) {
            code1 = (c & 0xf) - 10 + 'A';
         }
         c = (c >> 4) & 0xf;
         code0 = c + '0';
         if (c > 9) {
            code0 = c - 10 + 'A';
         }
         encodedString += '%';
         encodedString += code0;
         encodedString += code1;
      }
   }
   return encodedString;
}

void sendMessage(String message) {
   String encodedMessage = urlEncode(message);
   String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + encodedMessage;

   HTTPClient http;
   http.begin(url);
   http.addHeader("Content-Type", "application/x-www-form-urlencoded");

   int retries = 0;
   while (retries < MAX_RETRIES) {
      int httpResponseCode = http.GET();

      if (httpResponseCode == 200) {
         Serial.println("Message sent successfully");
         http.end();
         return;
      } else {
         Serial.println("Gagal mengirim pesan, mencoba ulang...");
         retries++;
         delay(1000);
      }
   }

   Serial.println("Pesan gagal terkirim setelah beberapa percobaan. Mereset perangkat...");
   resetDevice();
   http.end();
}

void checkFloodLevel(float distance) {
   int floodStage = 0;
   String message;

   if (distance < 25) {
      floodStage = 3;
      message = "Tenggelam, air sudah naik, jarak air ke alat " + String(distance, 2) + " cm!";
   } else if (distance < 70) {
      floodStage = 2;
      message = "Waspada, air sudah naik, jarak air ke alat " + String(distance, 2) + " cm!";
   } else if (distance < 100) {
      floodStage = 1;
      message = "Siaga, air sudah naik, jarak air ke alat " + String(distance, 2) + " cm!";
   }

   if (floodStage > lastFloodStage) {
      sendMessage(message);
      lastFloodStage = floodStage;
   } else if (floodStage < lastFloodStage) {
      lastFloodStage = floodStage;
   }
}

void setup() {
   Serial.begin(115200);
   setupWiFi();
   setupTime();
   pinMode(trigPin, OUTPUT);
   pinMode(echoPin, INPUT);
}

void loop() {

   unsigned long currentMillis = millis();
   if (currentMillis - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
      lastMeasurementTime = currentMillis;

      float distance = measureDistance();
      if (distance == -1) {
         sensorFailureCount++;
         Serial.println("Gagal membaca sensor, mencoba ulang...");
      } else {
         sensorFailureCount = 0; // Reset jika pembacaan berhasil
         Serial.print("Distance (cm): ");
         Serial.println((distance * 100));

         sendDataToFirebase(distance);
         checkFloodLevel((distance * 100));
      }

      // Jika sensor gagal beberapa kali berturut-turut, reset perangkat
      if (sensorFailureCount >= MAX_SENSOR_FAILURES) {
         Serial.println("Sensor terus gagal membaca jarak. Mereset perangkat...");
         resetDevice();
      }
   }
}
