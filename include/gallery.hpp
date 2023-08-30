#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "esp_camera.h"

#include "analyser.hpp"

void initGallery();
//void updateTime();
void saveImage();
void activateStandaloneImageSaveCycle();
void deactivateStandaloneImageSaveCycle();
void reactivateStandaloneImageSaveCycle();
uint32_t getImageIntervalUs();
void imageSaveTickImplied();
uint8_t getImageMaxLittleIndex();
uint8_t getLittleImageIndex();
bool checkReadyForSave();
