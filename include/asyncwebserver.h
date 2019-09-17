#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>


extern Settings settings;
String lastWsMessage;
extern int relay;
extern bool toggleRelay(int state);
extern bool saveSettings(Settings sd);
extern bool restartRequired;
extern const char* version;

AsyncWebSocketClient * globalClient = NULL;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

// ---------------------- functions -------------------------------


String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

void handleNotFound(AsyncWebServerRequest *request) {
    String path = request->url();
    if (path.endsWith("/")) path += "index.htm";         // If a folder is requested, send the index file
    String contentType = getContentType(path);            // Get the MIME type
    if (SPIFFS.exists(path)) {                            // If the file exists
        File file = SPIFFS.open(path, "r");                 // Open it
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, contentType);
        // response->addHeader("Server","ESP Async Web Server");
        request->send(response);
        Serial.println("200\tGET\t" + path);
        file.close();                                       // Then close the file again
    } else {
        Serial.println("404\tGET\t" + path);
        request->send(404, "text/plain", "Not found");
    }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if(type == WS_EVT_CONNECT){
        Serial.println("Websocket client connection received");
        globalClient = client;
        client->text(lastWsMessage);
    
    } else if(type == WS_EVT_DISCONNECT){
        globalClient = NULL;
        Serial.println("Client disconnected");
    }
}


// ----------------------------------------------------------------

void WifiSendtatus(String message){
    lastWsMessage = message;
    if(globalClient != NULL && globalClient->status() == WS_CONNECTED){
        globalClient->text(lastWsMessage);
        // globalClient->text();
    }
}


void startWifi() {
    // SPIFFS.begin();
    //######## config WIFI #########

    if(String(settings.wifiSSID)=="" || settings.runSetup==true){
        Serial.println("Running SetupMode");
        WiFi.hostname("GaragePackSetup");

        WiFi.persistent(false);
        // disconnect sta, start ap
        WiFi.disconnect(); //  this alone is not enough to stop the autoconnecter
        WiFi.mode(WIFI_AP);
        WiFi.persistent(true);

        WiFi.softAP("GaragePackSetup");
        
        Serial.println();
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
        // if (WiFi.wai() != WL_CONNECTED) {
        //     Serial.printf("WiFi AP Failed!\n");
        //     return;
        // }
    } else {
        //######## config WIFI #########
        WiFi.hostname(settings.wifiHostname);
        WiFi.mode(WIFI_STA);

        
        WiFi.begin(settings.wifiSSID, settings.wifiPassword);
        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
            Serial.printf("WiFi Failed!\n");
            return;
        }

        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }




    Serial.print("Hostname: ");
    Serial.println(WiFi.hostname());

    //######## WIFI routes #########


    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", lastWsMessage);
    });

    server.on("/door", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String action;
        if (request->hasParam("action")) {
            action = request->getParam("action")->value();
        } 

        if(action == "on" || action == "open"){
            request->send(200, "application/json", "true");
            toggleRelay(1);
        }else if (action == "off" || action == "close"){
            request->send(200, "application/json", "false");
            toggleRelay(0);
        } else {
            request->send(400, "application/json", "invalid command. use on, off, open,close");
        }
    });

    server.on("/restart", HTTP_GET, [] (AsyncWebServerRequest *request) {
        Serial.println("restarting");
        request->send(200, "application/json", "true");
        ESP.restart();
    });

    server.on("/settings", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(200, "application/json", SettingsToJson(settings));
    });

    // Send a POST request to <IP>/post with a form field message set to <message>
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam("body", true)) {
            message = request->getParam("body", true)->value();
        } else {
            request->send(400, "application/json", "no body key sent");
            return;
        }
        Settings sp = JsonToSettings(message);
        sp.runSetup = false; //overwrite privously set runsetup
        if(saveSettings(sp)){
            request->send(200, "application/json", "true");
            ESP.restart();
        } else {
            request->send(500, "application/json", "wrting settinsg failed");
        }
        
    });

    // --------------- UPDATE FIRMWARE ----------------------
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", version);
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", serverIndex);
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        // the request handler is triggered after the upload has finished... 
        // create the response, add header, and send response
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError())?"FAIL":"OK");
        response->addHeader("Connection", "close");
        response->addHeader("Access-Control-Allow-Origin", "*");
        restartRequired = true;  // Tell the main loop to restart the ESP
        request->send(response);
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        //Upload handler chunks in data
        
        if(!index){ // if index == 0 then this is the first frame of data
        Serial.printf("UploadStart: %s\n", filename.c_str());
        Serial.setDebugOutput(true);
        
        // calculate sketch space required for the update
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
            Update.printError(Serial);
        }
        Update.runAsync(true); // tell the updaterClass to run in async mode
        }

        //Write chunked data to the free sketch space
        if(Update.write(data, len) != len){
            Update.printError(Serial);
        }
        
        if(final){ // if the final flag is set then this is the last frame of data
        if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u B\nRebooting...\n", index+len);
            } else {
            Update.printError(Serial);
            }
            Serial.setDebugOutput(false);
        }
    });

    server.onNotFound(handleNotFound);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // #######################

    
    server.begin();
}