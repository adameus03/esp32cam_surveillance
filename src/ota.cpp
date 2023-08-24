#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include "OTACredentials.hpp"

#define OTA_INTERVAL_US 30000

volatile bool isTimeForOTAHandle = false;

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
