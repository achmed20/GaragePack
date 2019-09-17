#include <Arduino.h>

#include <Ultrasonic.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <structs.h>
#include <garagestatus.h>
#include <asyncwebserver.h>
#include <mqtt.h>

Settings settings;
// const char* ssid = "BND Servicemobil";
// const char* password = "bfb0zekczb";

// defines pins numbers
int relay = D5;
int pirPin = D7;
Ultrasonic ultrasonic1(D1, D2, 20000UL);

// unsigned long int relayMode = 500;  //0=toggle, >0=delay ms for push button emulation
bool relayState = LOW;
unsigned long relayTimer = 0;

const char* version = "0.8";

bool debug = false;
sensordata sensors;
int prevChecksum = 9;
unsigned long nextScan=0;
unsigned long scanTime = 100;

bool restartRequired = false;  
const uint setupAddr = 0;

// if ultrasonic sensor is <maxDistanceToConfig> for <configAry[seconds]>, start config portal
int maxDistanceToConfig = 10; 
unsigned int SecondsToConfig = 10;
unsigned long configStartTime = 0;

// failsafes and wifi checking values
unsigned int restartAfterWifiError = 300;
unsigned long timeToRestart = 0;
GarageStatus garagestatus;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
String currentTime = "";

// -------------------- internal functions ---------------------
bool saveSettings(Settings sd){
  File file = SPIFFS.open("/settings.json", "w");
  if(!file){
     Serial.println("There was an error opening the file for writing");
     return false;
  }
  if(file.print(SettingsToJson(sd))) {
    Serial.println("Settings written");
  }else {
      Serial.println("writing settings failed");
      return false;
  }
  return true;

}

int getDistance() { 
  return ultrasonic1.read(CM);
}

bool getMotion(){
  int val = digitalRead(pirPin);
  //low = no motion, high = motion
  if (val == LOW)
  {
    return false;
  }
  return true;
}

void checkRelay(){
    if(relayTimer>0 && millis() > relayTimer){
      Serial.println("{relay: false, mode: "+String(settings.relayMode)+"}");
      digitalWrite(relay, LOW);
      relayTimer = 0;
    }
}

bool toggleRelay(int state){
  bool cState = false;
  if(settings.relayMode==0){
    switch (state) {
      case 0:
        cState = LOW;
        break;
      case 1:
        cState = HIGH;
        break;
      case 2:
        cState = !relayState;
        break;
    }
    relayState = cState;
    digitalWrite(relay, cState);
    return cState;
  } else {

      Serial.println("{relay: true, mode: "+String(settings.relayMode)+"}");
    digitalWrite(relay, HIGH);
    relayTimer = millis() + settings.relayMode;
    return true;
  }
}

int getStatusChecksum(Status s){
  int ret = 0;
  if(s.open){
    ret+=1;
  }
  if(s.car){
    ret+=10;
  }
  if(s.motion){
    ret+=100;
  }
  return ret;
}

// ------------------------------ Setup -------------------------------------
void setup() {
  Serial.begin(115200); // Starts the serial communication
  
  Serial.print("Garagepack Version ");
  Serial.println(version);

  pinMode(relay, OUTPUT);

  //read config
  SPIFFS.begin();
  File f = SPIFFS.open("/settings.json", "r");
  if (!f) {
     Serial.println("loading settings failed");
  }else {
    String jdata = f.readString();
    settings = JsonToSettings(jdata);

    Serial.println("Loading settings");
    
    garagestatus.distanceDoorOpen = settings.doorDistanceOpen;
    garagestatus.distanceDoorClosed = settings.doorDistanceClosed;
   
  } 

  // delay(1000);
  startWifi();
  Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("Wifi connection error! restarting in seconds " + String(restartAfterWifiError));
    timeToRestart = millis() + (restartAfterWifiError * 1000);
  }

  timeClient.begin();
  if(String(settings.mqttHost)!=""){
    setupMqtt();
  } else {
    Serial.println("MQTT host not set, skipping");
  }
}

// ------------------ main loop

void loop() {
  
  // check wifi
  if ( !settings.runSetup and (WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_DISCONNECTED))
  {
    Serial.println("connection to WiFi lost, restarting");
    restartRequired = true;
  }

  // check for restart flag or restart timer
  if (restartRequired || (timeToRestart>0 and timeToRestart< millis() )){  // check the flag here to determine if a restart is required
    Serial.printf("Restarting ESP\n\r");
    restartRequired = false;
    ESP.restart();
  }

  if(String(settings.mqttHost)!=""){
    mqttLoop();
  }

  // get time for mqtt and mserial messages;
  currentTime = timeClient.getFormattedTime();

  bool stable = false;
  checkRelay(); // cant use delay because of asyncwebserver
  sensors.lastUpdate = millis();

  // if PIR enabled, initialize
  if(settings.hasPIR){
    sensors.motion = getMotion();
  }

  //dont scan for distance all the time (echos?)
  if(nextScan<=millis()){
    nextScan = millis() + scanTime;
    sensors.distance = getDistance();
    stable = garagestatus.feed(sensors);
    if(debug){
      Serial.println(sensors.distance);
    }
  }

  // -----  start config? check here
  if(sensors.distance <= maxDistanceToConfig && sensors.distance>0){
    if(configStartTime==0){
      configStartTime = millis();
      Serial.println("Config mode initializing");
    } 
  } else if (configStartTime>0) {
      Serial.println("Config mode cancled");
      configStartTime = 0;
  }
  
  // if scandistance lower <maxDistanceToConfig> for <SecondsToConfig> seconds
  // start config mode
  if(configStartTime >0 && (millis()-configStartTime)/1000 >= SecondsToConfig){
    configStartTime = 0;
    Serial.println("Starting Config Portal)");
    settings.runSetup = true; //save in settings, will be reset once settings have been saved via UI aggain
    saveSettings(settings);
    restartRequired = true;
  }
  // -------------------------------


  if(stable){ //only submit values if readings are stable
    String strStatus = StatusToJson(garagestatus.getStatus());
    char charStatus[255];
    strStatus.toCharArray(charStatus, 255);

    if(getStatusChecksum(garagestatus.getStatus()) != prevChecksum){
      mqttPublish(charStatus);
      Serial.println(strStatus);
      WifiSendtatus(strStatus);
    }
    prevChecksum = getStatusChecksum(garagestatus.getStatus());

  }

}
