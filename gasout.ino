#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define AWS_IOT_PUBLISH_TOPIC   "gas-out-topic"
 
const int THRESHOLD_HIGH=50; // Led Vermelho, Nível Alto
const int THRESHOLD_MID=25; // Led Amarelo, Nível Médio
const int PUBLISH_COOLDOWN = 20;

const byte Sensor1AnalogPin=A0;
float sensor1ValuePPM=0;
const byte Sensor2AnalogPin=5;
float sensor2ValuePPM=0;

const int RedLedPin1=5;
const int YellowLedPin1=4;
const int GreenLedPin1=2;

const int RedLedPin2=14;
const int YellowLedPin2=12;
const int GreenLedPin2=13;


const int FlashButtonPin=0;

unsigned long publishTime=0;
int flashButton=1;

int flickerTimer=0;
 
WiFiClientSecure net;
 
BearSSL::X509List cert(AWS_CERT_CA);
BearSSL::X509List client_crt(AWS_CERT_CRT);
BearSSL::PrivateKey key(AWS_CERT_PRIVATE);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1510592825;

void setup()
{
  pinMode(0, INPUT_PULLUP); //botao FLASH
  
  pinMode(RedLedPin1, OUTPUT);
  pinMode(YellowLedPin1, OUTPUT);
  
  digitalWrite(RedLedPin1, HIGH);
  digitalWrite(YellowLedPin1, HIGH);
  
  Serial.begin(115200);
  connectAWS();
}


void connectAWS(){
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }
 
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME)){
    Serial.print(String(client.state()));
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  Serial.println("AWS IoT Connected!");
}
 
void NTPConnect(void){//Network Time Protocol
  Serial.print("Setting time using SNTP");
  configTime(3 * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
void loop(){
  if(!client.connected()){
   connectAWS();
  }else{
    sensor1ValuePPM = getMethanePPM(analogRead(Sensor1AnalogPin));
    flashButton=digitalRead(FlashButtonPin);
    
    Serial.print("Methane 1 PPM: ");
    Serial.println(String(sensor1ValuePPM) + " [" + analogRead(Sensor1AnalogPin) + "]");
    Serial.println("--");
    
    if (sensor1ValuePPM >= THRESHOLD_HIGH){
      flickerLed(RedLedPin1);
    }else{
      digitalWrite(RedLedPin1, LOW);
    }

    if (sensor1ValuePPM >= THRESHOLD_MID){
      flickerLed(YellowLedPin1);
    }else{
      digitalWrite(YellowLedPin1, LOW);
    }

    publishMessageSensor(sensor1ValuePPM);

    delay(1000);
    if(publishTime > 0){
      publishTime--;
    }
    client.loop();
  }
}

void flickerLed(float ledPin){
  digitalWrite(ledPin, HIGH);
}

float getMethanePPM(float a0){
   float v_o = a0 * 5 / 1023; // convert reading to volts
   float R_S = (5-v_o) * 1000 / v_o; // apply formula for getting RS
   float PPM = pow(R_S/945,-2.95) * 1000; //apply formula for getting PPM
   return PPM; // return PPM value to calling function
}

void publishMessageSensor(float sensorValue){
  if(publishTime <= 0){
    publishTime = PUBLISH_COOLDOWN;
    StaticJsonDocument<200> doc;
    StaticJsonDocument<200> message;
    
    message["sensorValue"] = sensorValue;
    message["roomName"] = "Cozinha";
    message["email"] = "test@email.com";
    
    doc["message"] = message;
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
    Serial.println("MQTT Published!: "+String(sensorValue));
  }else{
    Serial.println("Not published! Cooldown: "+String(publishTime));
  }
}
