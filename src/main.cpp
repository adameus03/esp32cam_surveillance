//https://RandomNerdTutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/
//https://plociennik.info/index.php/informatyka/systemy-wbudowane/esp8266

/*
  TODO
  [ ] Allow multiple clients to connect to the webservers #0
  [ ] Handle the limited storage of the uSD card (delete the oldest file(s)) #1
  [x] OTA upload  #2
  [x] timer interrupts to store photos 11.08.2023 #3
  [ ] OTA debug(override serial) #4
  [x] OTA upload using timer interrupt #5
  [ ] Watchdog #6
*/

/*
  OTA uses timer 0
  Gallery uses timer 1
*/

#include <ArduinoOTA.h>

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h"

#include "camera.hpp"
#include "webserver.hpp"
#include "gallery.hpp"
//#include "synchro.hpp"
#include "ota.hpp" //

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  esp_err_t err = initialize_camera();
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //initializeSynchro();
  initializeWebServer();

  initOTA();
  startHandlingOTA();

  delay(3000);
  initGallery();
  Serial.println("After initGallery()");
  activateStandaloneImageSaveCycle();
  Serial.println("After activateStandaloneImageSaveCycle()");
  
  startWebServer(); //C//
}

void loop() {
  if(checkReadyForSave()){
    imageSaveTickImplied();
  }
  if(checkIfTimeForOTAHandle()){
    OTATickImplied();
  }
  else delay(10); //////

  //Serial.println(heap_caps_get_free_size(0)); //D
  //updateTime(); //concurrent r/w??
}