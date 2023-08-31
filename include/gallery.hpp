#include "FS.h"
#include "SD_MMC.h"
#include "time.h"
#include "esp_camera.h"

#include "analyser.hpp"
//#include <bits/stl_vector.h>
#include <vector>

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

/*uint32_t*/void setBaseImage(File baseImageFile);
File getImage(uint32_t index);

bool rewindGallery();
const char *getCurrImageName();
File getNextImage();
File getPrevImage();
File getFirstHistoryImage();
File getLastHistoryImage();

std::vector<String>* getMasterEntries();
