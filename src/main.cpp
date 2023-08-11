//https://RandomNerdTutorials.com/esp32-cam-video-streaming-web-server-camera-home-assistant/

/*
  TODO
  [ ] Allow multiple clients to connect to the webservers
  [ ] Handle the limited storage of the uSD card (delete the oldest file(s))
  [ ] OTA upload & debug(override serial)
  [ ] timer interrupts to store photos 11.08.2023
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
#include "synchro.hpp"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  esp_err_t err = initialize_camera();
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  initializeSynchro();
  initializeWebServer();
  initGallery();

  startWebServer();
  //pinMode(4, INPUT);
  //ArduinoOTA.begin();
  //ArduinoOTA.setPassword("TPXBzUS3");
}

void loop() {
  /*for (int i = 0; i < 10; i++){
    ArduinoOTA.handle();
    delay(33);
  }*/
  ArduinoOTA.handle();
  Serial.println("Before saveImage()");
  saveImage(); //// !moved to webserver stream_handler's loop, where it is executed in intervals of 1 sec.
  Serial.println("After saveImage()");
  delay(300);

  //delay(333);
  //delay(1);
}