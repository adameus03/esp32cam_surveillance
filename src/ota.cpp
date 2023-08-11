#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include "OTACredentials.hpp"

void initOTA(){
  ArduinoOTA.setHostname(OTA_MDNS_HOSTNAME);
  ArduinoOTA.setPasswordHash(OTA_PASSWD_HASH);
  
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