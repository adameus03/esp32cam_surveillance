
#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "esp_camera.h"

#include "synchro.hpp"

#include "ArduinoOTA.h"

const char *timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // Europe/Warsaw, https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
String storageDirectory;
uint32_t imageIndex = 0;

void setTimezone(String timezone){
  Serial.printf("Setting Timezone to %s\n", timezone);
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(){
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time. Trying again in 1 second...");
    for (int i = 0; i < 10; i++){
      ArduinoOTA.handle();
      delay(100);
    }
  }
  Serial.println("Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

String getDatetimeString(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  Serial.println(timeString);
  return String(timeString);
}

void initializeDirectoryStorage(){
  storageDirectory = "/" + getDatetimeString();
  while(!SD_MMC.mkdir(storageDirectory)){
    Serial.println("Failed to create directory. Retrying in 1 sec.");
    for (int i = 0; i < 10; i++){
      ArduinoOTA.handle();
      delay(100);
    }
  }
}

// Initialize the micro SD card
void initMicroSDCard(){
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
}

void initGallery(){
  initTime();
  initMicroSDCard();
  initializeDirectoryStorage();
}

// Take photo and save to microSD card
void saveImage(){
  // Take Picture with Camera

  while(!lock_fb()){
    ArduinoOTA.handle();
    Serial.println("+ saveImage lock FAIL! Delaying 100ms---");
    delay(100);
  }
  Serial.println("+ saveImage lock successfull");

  Serial.println("Before esp_camera_fb_get() in saveImage");
  camera_fb_t * fb = esp_camera_fb_get();
  Serial.println("After esp_camera_fb_get() in saveImage");
 
  //Uncomment the following lines if you're getting old pictures
  //esp_camera_fb_return(fb); // dispose the buffered image
  //fb = NULL; // reset to capture errors
  //fb = esp_camera_fb_get();
  
  if(!fb) { //neccesary with the lock?
    Serial.println("Camera capture failed in saveImage");
    Serial.println("Before esp_camera_fb_return(fb)");
    esp_camera_fb_return(fb);
    Serial.println("After esp_camera_fb_return(fb)");
    unlock_fb(); //!!!
    return;
  }

  // Path where new picture will be saved in SD Card
  String path = storageDirectory + "/frame_" + getDatetimeString() + "___" + imageIndex++ + ".jpg";
  Serial.printf("Picture file name: %s\n", path.c_str());
  
  // Save picture to microSD card
  //fs::FS &fs = SD_MMC; 
  //File file = fs.open(path.c_str(), FILE_WRITE);
  File file = SD_MMC.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.printf("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved: %s\n", path.c_str());
  }
  ArduinoOTA.handle();
  file.close();
  esp_camera_fb_return(fb);
  ArduinoOTA.handle();
  while(!unlock_fb()){
    Serial.println("- saveImage unlock FAIL! Delaying 100ms---");
    delay(100);
    ArduinoOTA.handle();
  }
  Serial.println("- saveImage unlock successfull");
}

