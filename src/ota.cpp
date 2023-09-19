#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include "WebServer.h" // for non-local OTA
#include "Update.h"    //also for non-local OTA

#include "confidential/OTACredentials.hpp"

#define OTA_INTERVAL_US 30000
#define PUBLIC_OTA_INTERVAL_US 3000

volatile bool isTimeForOTAHandle = false;
volatile bool isTimeForPubliOTAHandle = false;

WebServer publicOTAServer(9000);


void initOTA(){
  ArduinoOTA.setHostname(OTA_MDNS_HOSTNAME);
  //ArduinoOTA.setPasswordHash(OTA_PASSWD_HASH);  ////////////
  
  ArduinoOTA.onStart([](){
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  }).onEnd([](){
    Serial.println("\nEnd");
  }).onProgress([](unsigned int progress, unsigned int total){
    Serial.printf("Progress: %u%%\r", (progress/(total/100)));
  }).onError([](ota_error_t error){
    Serial.printf("Error[%u]: ", error);
    switch(error){
      case OTA_AUTH_ERROR:
        Serial.println("Auth failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("Begin failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("Connect failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("Receive failed");
        break;
      case OTA_END_ERROR:
        Serial.println("End failed");
        break;
      default:
        Serial.println("Other OTA error");
    }
  });

  Serial.println("ArduinoOTA before begin");
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA began");
}

void OTATickImplied(){
  //.Serial.println("Inside OTATickImplied()"); //CHECK THIS <-------
  isTimeForOTAHandle = false;
  ArduinoOTA.handle();
  //isTimeForOTAHandle = false;
}

void IRAM_ATTR onOTATimer(){
  //ArduinoOTA.handle();
  isTimeForOTAHandle = true;
}

/**
 * @brief timer 0 to attach an an interrupt. Run ArduinoOTA.handle() every 300ms
*/
void startHandlingOTA(){ 
  hw_timer_t *ota_timer = timerBegin(0, 80, true); //ChatGPT said ESP32-S timer base frequency is 80MHz
  timerAttachInterrupt(ota_timer, &onOTATimer, true);
  timerAlarmWrite(ota_timer, OTA_INTERVAL_US, true);
  timerAlarmEnable(ota_timer);
}

bool checkIfTimeForOTAHandle(){
  return isTimeForOTAHandle;
}


/* NON LOCAL */

void publicOTATickImplied(){
  isTimeForPubliOTAHandle = false;
  publicOTAServer.handleClient();
}

void onPublicOTATimer(){
  isTimeForPubliOTAHandle = true;
}

void startHandlingPublicOTA(){
  publicOTAServer.on("/", HTTP_GET, []() {
    publicOTAServer.sendHeader("Connection", "close");
    publicOTAServer.send(200, "text/html", 
    #include "htdocs/publicOTA.html"
    );
  });
  publicOTAServer.on("/update", HTTP_POST, []() {
    publicOTAServer.sendHeader("Connection", "close");
    publicOTAServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = publicOTAServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  publicOTAServer.begin();

  hw_timer_t *public_ota_timer = timerBegin(2, 80, true);
  timerAttachInterrupt(public_ota_timer, &onPublicOTATimer, true);
  timerAlarmWrite(public_ota_timer, PUBLIC_OTA_INTERVAL_US, true);
  timerAlarmEnable(public_ota_timer);
}

void haltPublicOTA(){
  publicOTAServer.close();
  publicOTAServer.stop();
}

void resumePublicOTA(){
  publicOTAServer.begin();
}

bool checkIfTimeForPublicOTAHandle(){
  return isTimeForPubliOTAHandle;
}
