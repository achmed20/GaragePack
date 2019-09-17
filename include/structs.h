#include <ArduinoJson.h> 

struct Status {
    bool open;
    bool car;
    bool motion;
    int distance;
    unsigned long lastUpdate;
};

struct sensordata{
    int distance;
    bool motion;
    unsigned long lastUpdate;
} ;

struct Settings {
    int doorDistanceOpen = 20;
    int doorDistanceClosed = 180;
    int doorTimeToggle = 20;
    unsigned long int relayMode = 500;
    bool hasPIR = true;

    const char* wifiSSID = "";
    const char* wifiPassword = "";
    const char* wifiHostname = "GaragePack";
    int restartAfterWifiError = 120;

    const char* mqttHost = "192.168.12.200";
    int mqttPort = 1883;
    const char* mqttLogin = "";
    const char* mqttPassword = "";
    bool runSetup = false;
};

struct MqttCommand {
    const char* command;
    const char* value;
};

// --------------------------------------------------------------------

String StatusToJson(Status sd) {
    String output;
    StaticJsonDocument<200> doc;
    doc["open"] = sd.open;
    doc["car"] = sd.car;
    doc["motion"] = sd.motion;
    doc["distance"] = sd.distance;
    doc["lastupdate"] = sd.lastUpdate;
    serializeJson(doc, output);
    return output;
}

String sensordataToJson(sensordata sd) {
    String output;
    StaticJsonDocument<200> doc;
    doc["distance"] = sd.distance;
    doc["motion"] = sd.motion;
    serializeJson(doc, output);
    return output;
}

String SettingsToJson(Settings sd) {
    String output;
    StaticJsonDocument<2000> doc;

    doc["doorDistanceOpen"] = sd.doorDistanceOpen;
    doc["doorDistanceClosed"] = sd.doorDistanceClosed;
    doc["doorTimeToggle"] = sd.doorTimeToggle;
    doc["hasPIR"] = sd.hasPIR;
    doc["relayMode"] = sd.relayMode;
    doc["wifiSSID"] = sd.wifiSSID;
    doc["wifiPassword"] = sd.wifiPassword;
    doc["wifiHostname"] = sd.wifiHostname;
    doc["mqttHost"] = sd.mqttHost;
    doc["mqttLogin"] = sd.mqttLogin;
    doc["mqttPassword"] = sd.mqttPassword;
    doc["runSetup"] = sd.runSetup;
    doc["restartAfterWifiError"] = sd.restartAfterWifiError;

    serializeJson(doc, output);
    return output;
}

Settings JsonToSettings(String json){
    Settings sd;
    StaticJsonDocument<2000> doc;
    DeserializationError error = deserializeJson(doc, json);

    // Test if parsing succeeds.
    if (error) {
        Serial.print(F("deserializeJson settings"));
        Serial.println(error.c_str());
    } else {
        sd.doorDistanceOpen = doc["doorDistanceOpen"];
        sd.doorDistanceClosed = doc["doorDistanceClosed"];
        sd.doorTimeToggle = doc["doorTimeToggle"];
        sd.relayMode = doc["relayMode"];
        sd.hasPIR = doc["hasPIR"];
        sd.wifiSSID = doc["wifiSSID"];
        sd.wifiPassword = doc["wifiPassword"];
        sd.wifiHostname = doc["wifiHostname"];
        sd.mqttHost = doc["mqttHost"];
        sd.mqttLogin = doc["mqttLogin"];
        sd.mqttPassword = doc["mqttPassword"];
        sd.runSetup = doc["runSetup"];
        sd.restartAfterWifiError = doc["restartAfterWifiError"];
    }
    return sd;
}


MqttCommand JsonToMqttCommand(String json){
    MqttCommand sd;
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, json);

    // Test if parsing succeeds.
    if (error) {
        Serial.print(F("deserializeJson MqttCommand"));
        Serial.println(error.c_str());
    } else {
        sd.command = doc["command"];
        sd.value = doc["value"];
    }
    return sd;
}