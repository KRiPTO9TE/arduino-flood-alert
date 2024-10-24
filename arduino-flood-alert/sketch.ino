#include <WiFi.h>    
#include <HTTPClient.h>
#include <UrlEncode.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

String phoneNumber = "YOUR_PHONE_NUMBER";
String apiKey = "YOUR_WHATSAPP_API_KEY";

const int trigPin = 5;
const int echoPin = 18;

const double bridgeHeightInM = 8;

//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

int lastFloodStage;
int currentFloodStage;

long duration;
float distanceCm;
float distanceM;


void sendMessage(String message){
  // Data to send with HTTP POST
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);    
  HTTPClient http;
  http.begin(url);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200){
    Serial.print("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  // Free resources
  http.end();
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  lastFloodStage = 0;

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  // Send Message to WhatsAPP
  
}

void loop() {
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;
  
  // Convert to inches
  distanceM = distanceCm / 100;

  if(distanceCm < 50){
    currentFloodStage = 1;
    if(lastFloodStage != currentFloodStage){
      sendMessage("Tenggelam bro");
    }
  } else if (distanceCm < 100) {
    currentFloodStage = 2;
    if(lastFloodStage != currentFloodStage){
      sendMessage("Dikit lagi jadi atlantis");
    }
  } else if (distanceCm < 200) {
    currentFloodStage = 3;
    if(lastFloodStage != currentFloodStage){
      sendMessage("Cek air sudah mulai naik");
    }
  } else {
    currentFloodStage = 0;
  }
  
  // Prints the distance in the Serial Monitor
  Serial.println(lastFloodStage != currentFloodStage);
  Serial.println(currentFloodStage);
  Serial.println(lastFloodStage);
  lastFloodStage = currentFloodStage;
  Serial.print("Distance (cm): ");
  Serial.println((bridgeHeightInM * 100) - distanceCm);
  Serial.print("Distance (m): ");
  Serial.println(bridgeHeightInM - distanceM);
  
  delay(1000);
}