
#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "esp_camera.h"

//#include "synchro.hpp"
#include "camera.hpp"

#include "analyser.hpp"

//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <bits/stl_vector.h>
#include <vector>
//#include <bits/stl_iterator_base_funcs.h>

#include "alarm_broker.hpp"

#include "formatter.hpp"

//#define IMAGE_INTERVAL_US 500000
//#define IMAGE_MAX_LITTLE_INDEX 1
#define IMAGE_INTERVAL_US 300000
#define IMAGE_MAX_LITTLE_INDEX 2

hw_timer_t *imageSaveTimer = NULL;

volatile bool isReadyForSave = false;

bool standaloneImageSaveCycleActivated = false;

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

  if(!SD_MMC.exists("/master.txt")){
    SD_MMC.open("/master.txt", FILE_WRITE, true);
  }
}

// Initialize the micro SD card
void initMicroSDCard(){
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin(/*"/sdcard", true*/)){
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
    /*uint8_t *jpg_buf = NULL;
    size_t jpg_buf_len;
    bool jpeg_converted = frame2jpg(frameBuffer, 80, &jpg_buf, &jpg_buf_len);
    if(!jpeg_converted){
      Serial.println("JPEG compression failed in saveImage()");
    }*/
    ///</compression>


    file.write(frameBuffer->buf, frameBuffer->len); // payload (image), payload length
    //file.write(jpg_buf, jpg_buf_len);

    //.Serial.printf("Saved: %s\n", path.c_str());

    ///<compression free>
    //free(jpg_buf);
    ///</compression free>
  }

  file.close();

  file = SD_MMC.open("/master.txt", FILE_APPEND);
  file.write((uint8_t *)(path + "\n").c_str(), path.length() + 1);
  file.close();
}

//static File baseImage; //not used
//char *masterContent = nullptr;
std::vector<String> masterEntries;
uint32_t curr_viewing_index = 0xffffffff;

void deactivateStandaloneImageSaveCycle(); //@@
void reactivateStandaloneImageSaveCycle(); //@@

/**
 * @brief Sets index of the baseImageFile as curr_viewing_index
*/
void setBaseImage(File baseImageFile){
  /*if(String(baseImage.name()).startsWith("/")){
    Serial.println("DEBUG baseImage.name() OK");
  }
  else {
    Serial.println("DEBUG baseImage.name() NOT OK");
  }*/

  //baseImage = baseImageFile; //not used
  deactivateStandaloneImageSaveCycle();//@@
  File masterFile = SD_MMC.open("/master.txt", FILE_READ);
  uint32_t counter = 10;
  while((!masterFile) && counter--){//@@
    Serial.println("Failed to open master.txt, retrying in 1 sec....");
    delay(1000);
    masterFile = SD_MMC.open("/master.txt", FILE_READ);
  }
  //size_t masterFile_size = masterFile.size();
  //masterContent = (char*)malloc(masterFile_size);

  //masterFile.readBytes(masterContent, masterFile_size); // note: not null-terminated

  char temp[50];

  uint32_t index = 0x0;
  //uint32_t ret_index = 0xffffffff;

  while(masterFile.available()){
    size_t n = masterFile.readBytesUntil('\n', temp, 50);
    //Serial.println("setBaseImage: [" + String(n) + "] " + baseImage.name()); // DEBUG
    temp[n] = '\0';
    Serial.println("setBaseImage: [" + String(n) + "] " + String(temp)); // DEBUG

    if(!strcmp(/*@@baseImage*/baseImageFile.name(), ((char*)temp)+0x1)){// +0x1 to omit the initial "/"
      //ret_index = index;
      curr_viewing_index = index;
    }
    masterEntries.push_back(String(temp));
    index++;
  }
  masterFile.close();
  reactivateStandaloneImageSaveCycle();//@@
  //String(masterContent)
  //curr_viewing_index = ret_index;
}

/**
 * @brief Obsolete / not used
*/
File getImage(uint32_t index){
  if(index >= 0 && index < masterEntries.size()){
    curr_viewing_index = index;
    return SD_MMC.open(masterEntries[index], FILE_READ);
  }
  return (File)NULL;
  
}

/**
 * Sets first image as base image & curr_viewing_index = 0x0
*/
bool rewindGallery(){
  
  masterEntries.clear();
  if(!masterEntries.empty()){
    Serial.println("Theyre not empty");
    return false;
  }
  Serial.println("Theyre empty");


  
  deactivateStandaloneImageSaveCycle();//@@
  File masterFile = SD_MMC.open("/master.txt", FILE_READ);

  uint32_t counter = 10;
  while((!masterFile) && counter--){//@@
    Serial.println("Failed to open master.txt, retrying in 1 sec....");
    delay(1000);
    masterFile = SD_MMC.open("/master.txt", FILE_READ);
  }

  char temp[50];
  while(masterFile.available()){
    size_t n = masterFile.readBytesUntil('\n', temp, 50);
    if(n==0){
      Serial.println("masterFile.readBytesUntil returned 0 ! Breaking the loop, therefore...");
      Serial.println("Position: " + String(masterFile.position()));
      masterFile.close();
      break;
    }
    temp[n] = '\0';
    Serial.println("rewindGallery: [" + String(n) + "] " + String(temp)); // DEBUG

    masterEntries.push_back(String(temp));
  }
  masterFile.close();
  reactivateStandaloneImageSaveCycle();//@@
  if(masterEntries.size() > 0){
    curr_viewing_index = 0x0;
    return true;
  }
  return false;
}

const char* getCurrImageName(){
  if(curr_viewing_index >= 0 && curr_viewing_index < masterEntries.size()){
    return masterEntries[curr_viewing_index].c_str();
  }
  return NULL;
}
File getNextImage(){
  if(curr_viewing_index != 0xffffffff){
    curr_viewing_index++;
    if(curr_viewing_index >= masterEntries.size()){
      return (File)NULL;
    }
    return SD_MMC.open(masterEntries[curr_viewing_index], FILE_READ);
  }
  return (File)NULL;
}

File getPrevImage(){
  if(curr_viewing_index != 0xffffffff){
    if(curr_viewing_index == 0x0){
      return (File)NULL;
    }
    curr_viewing_index--;
    return SD_MMC.open(masterEntries[curr_viewing_index], FILE_READ);
  }
  return (File)NULL;
}

File getFirstHistoryImage(){
  if(masterEntries.empty()){
    return (File)NULL;
  }
  curr_viewing_index = 0x0;
  return SD_MMC.open(masterEntries[0x0], FILE_READ);
}

File getLastHistoryImage(){
  if(masterEntries.empty()){
    return (File)NULL;
  }
  uint32_t last_index = masterEntries.size() - 0x1;
  curr_viewing_index = last_index;
  return SD_MMC.open(masterEntries[last_index], FILE_READ);
}

std::vector<String>* getMasterEntries(){
  return &masterEntries;
}

/*void releaseMasterContent(){
  free(masterContent);
}*/

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
    if(!checkApproval(frameBuffer->buf, frameBuffer->len, frameBuffer->width, frameBuffer->height)){
      Serial.println("release_fb();");
      release_fb();
      return;
    }
    else {
      //{{{notify user}}}, {{{play alarm}}}
      notifyAlarmSoundSystem();

    }
  }

  Serial.println("saveImage();");
  saveImage();

  Serial.println("release_fb();");
  release_fb();
}

void activateStandaloneImageSaveCycle(){
  standaloneImageSaveCycleActivated = true;
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
  standaloneImageSaveCycleActivated = false;
  timerAlarmDisable(imageSaveTimer);
}

void reactivateStandaloneImageSaveCycle(){
  standaloneImageSaveCycleActivated = true;
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

esp_err_t formatStorage(){
  esp_err_t res;

  if(standaloneImageSaveCycleActivated){
    deactivateStandaloneImageSaveCycle();
    Serial.println("Delay before format");
    delay(3000);
    Serial.println("Formatting...");
    res = format_sdcard(SD_MMC.getCard());
    reactivateStandaloneImageSaveCycle();
  }
  else {
    res = format_sdcard(SD_MMC.getCard());
  }

  return res;
}