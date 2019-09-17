
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


extern Settings settings;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
char mqttTopicState[50] = "/state";
char mqttTopiCommand[50] = "/cmnd";


void callback(char* topic, byte* payload, unsigned int length) {
  char msgin[200] = "";;
  for (unsigned int i = 0; i < length; i++) {
    msgin[i] = (char)payload[i];
  }
  // if(mqttTopiCommand == topic)
  {
    MqttCommand cmd;
    cmd = JsonToMqttCommand(msgin);
    Serial.print("MQTT Command: ");
    // Serial.print(topic);
    Serial.println(msgin);
    if(String(cmd.command) == "open"){
        if(String(cmd.value)=="on"){
          toggleRelay(true);
        }
        if(String(cmd.value)=="off"){
          toggleRelay(false);
        }
    } else {
        Serial.println(" Invalid");
    }
  }




}

void reconnect() {
  // Loop until we're reconnected
  // while (!client.connected())
  if(!client.connected()){
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      Serial.print("MQTT publish: ");
      Serial.println(mqttTopicState);
      Serial.print("MQTT commands: ");
      Serial.println(mqttTopiCommand);

      client.subscribe(mqttTopiCommand);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("Failed, trying aggain next time");
      // Wait 5 seconds before retrying
      // delay(5000);
    }
  }
}

void setupMqtt() {
  sprintf(mqttTopiCommand, "home/%s/cmnd", settings.wifiHostname);
  sprintf(mqttTopicState, "home/%s/state", settings.wifiHostname);

  client.setServer(settings.mqttHost, settings.mqttPort);
  client.setCallback(callback);
}

void mqttPublish(char msg[255] ){
    if (client.connected()) {
        Serial.print("MQTT Publish: ");
        // Serial.println(msg);
        client.publish(mqttTopicState, msg);
    }
}

void mqttLoop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

}