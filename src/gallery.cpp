
#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "esp_camera.h"

//#include "synchro.hpp"
#include "camera.hpp"

#include "analyser.hpp"

//#define IMAGE_INTERVAL_US 500000
//#define IMAGE_MAX_LITTLE_INDEX 1
#define IMAGE_INTERVAL_US 300000
#define IMAGE_MAX_LITTLE_INDEX 2

hw_timer_t *imageSaveTimer = NULL;

volatile bool isReadyForSave = false;

//tm timeinfo;
const char *timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // Europe/Warsaw, https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
String storageDirectory;
uint32_t imageIndex = 0; //neccessary?
volatile uint8_t littleImageIndex = 2;

void setTimezone(String timezone){
  //Serial.printf("Setting Timezone to %s\n", timezone);
  Serial.println("Setting timezone to " + timezone);
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(){
  struct tm timeinfo;
  Serial.println("Setting up time");
  configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time. Trying again in 1 second...");
    delay(1000);
  }
  Serial.println("Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

/*void updateTime(){
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
}*/

String getDatetimeString(){
  struct tm timeinfo;
  //timeinfo.tm_isdst

  //D//Serial.println("if(!getLocalTime(&timeinfo))");
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "";
  }

  /*time_t now;
  Serial.println("localtime_r(&now, &timeinfo);");
  localtime_r(&now, &timeinfo);*/

  //D//Serial.println("char timeString[20];");
  char timeString[20];

  //D//Serial.println("strftime(timeString, sizeof(timeString), \"%Y-%m-%d_%H-%M-%S\", &timeinfo);");
  strftime(timeString, sizeof(timeString), "%Y-%m-%d_%H-%M-%S", &timeinfo);

  //D//Serial.println(timeString);

  //D//Serial.println("return String(timeString);");
  return String(timeString);
}

void initializeDirectoryStorage(){
  /*storageDirectory = "/" + getDatetimeString();
  while(!SD_MMC.mkdir(storageDirectory)){
    Serial.println("Failed to create directory. Retrying in 1 sec.");
    delay(1000);
  }*/
  storageDirectory = "/";
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
  // Path where new picture will be saved in SD Card
  //Serial.println("String dateTimeString = getDatetimeString();");
  String dateTimeString = getDatetimeString();

  //Serial.println("String path = storageDirectory + \"/frame_\" + dateTimeString + \"___\" + imageIndex++ + \".jpg\";");
  String path = /*storageDirectory+*/ "/frame_" + dateTimeString + "_" + littleImageIndex + ".jpg";

  Serial.println("Save file at " + path);

  //.Serial.println("String path = storageDirectory + \"/frame_\" + imageIndex++ + \".jpg\";");
  ////String path = storageDirectory + "/frame_" + imageIndex++ + ".jpg";

  //.Serial.printf("Picture file name: %s\n", path.c_str());

  // Save picture to microSD card
  // fs::FS &fs = SD_MMC;
  // File file = fs.open(path.c_str(), FILE_WRITE);

  //.Serial.println("File file = SD_MMC.open(path.c_str(), FILE_WRITE);"); //D
  Serial.println("Begin open file");
  File file = SD_MMC.open(path.c_str(), FILE_WRITE);
  Serial.println("File open");

  //.Serial.println("if(!file)"); //D
  if(!file){
    //.Serial.printf("Failed to open file in writing mode");
  } 
  else {
    //.Serial.println("ELSE"); //DD

    //.Serial.println("camera_fb_t *frameBuffer = get_fb_no_reserve();");
    camera_fb_t *frameBuffer = get_fb_no_reserve(); //CHECK DOCS FOR ?

    if(!frameBuffer){
      Serial.println("NULL frame buffer detected in saveImage()");
      //reset camera??
      return;
    }

    //.Serial.println("file.write(frameBuffer->buf, frameBuffer->len);");


    ///<compression>
    uint8_t *jpg_buf = NULL;
    size_t jpg_buf_len;
    bool jpeg_converted = frame2jpg(frameBuffer, 80, &jpg_buf, &jpg_buf_len);
    if(!jpeg_converted){
      Serial.println("JPEG compression failed in saveImage()");
    }
    ///</compression>


    //file.write(frameBuffer->buf, frameBuffer->len); // payload (image), payload length
    file.write(jpg_buf, jpg_buf_len);

    //.Serial.printf("Saved: %s\n", path.c_str());

    ///<compression free>
    free(jpg_buf);
    ///</compression free>
  }

  file.close();
}

void IRAM_ATTR onImageSaveTimer(){
  //Serial.println("Image tick");
  uint8_t littleImageIndexCopy = littleImageIndex;
  littleImageIndexCopy++;
  littleImageIndexCopy %= (IMAGE_MAX_LITTLE_INDEX+1);

  isReadyForSave = true;
  littleImageIndex = littleImageIndexCopy;
}



void imageSaveTickImplied(){

  isReadyForSave = false;

  //.Serial.println("freeze_load_fb();");
  freeze_load_fb();

  if(checkCaching()){
    camera_fb_t *frameBuffer = get_fb_no_reserve();
    if(frameBuffer == NULL){
      Serial.println("NULL frameBuffer detected in imageSaveTickImplied()");
      Serial.println("release_fb();");
      release_fb();
    }
    if(!checkApproval(frameBuffer->buf, frameBuffer->len, frameBuffer->width)){
      Serial.println("release_fb();");
      release_fb();
      return;
    }
  }

  Serial.println("saveImage();");
  saveImage();

  Serial.println("release_fb();");
  release_fb();
}

void activateStandaloneImageSaveCycle(){
  Serial.println("imageSaveTimer = timerBegin(1, 80, true);");
  imageSaveTimer = timerBegin(1, 80, true);

  Serial.println("timerAttachInterrupt(imageSaveTimer, &onImageSaveTimer, true);");
  timerAttachInterrupt(imageSaveTimer, &onImageSaveTimer, true);

  Serial.println("timerAlarmWrite(imageSaveTimer, IMAGE_INTERVAL_US, true);");
  timerAlarmWrite(imageSaveTimer, IMAGE_INTERVAL_US, true);

  Serial.println("timerAlarmWrite(imageSaveTimer, IMAGE_INTERVAL_US, true);");
  timerAlarmEnable(imageSaveTimer);
}

void deactivateStandaloneImageSaveCycle(){
  timerAlarmDisable(imageSaveTimer);
}

void reactivateStandaloneImageSaveCycle(){
  timerAlarmEnable(imageSaveTimer);
}

uint32_t getImageIntervalUs(){
  return IMAGE_INTERVAL_US;
}

uint8_t getImageMaxLittleIndex(){
  return IMAGE_MAX_LITTLE_INDEX;
}

uint8_t getLittleImageIndex(){
  return littleImageIndex;
}

bool checkReadyForSave(){
  return isReadyForSave;
}

/*void formatStorage(){
  SD.for
}*/