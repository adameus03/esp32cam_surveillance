#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

SemaphoreHandle_t fb_mutex;

void initializeSynchro(){
    fb_mutex = xSemaphoreCreateMutex();
}

bool lock_fb(){
    return xSemaphoreTake(fb_mutex, portMAX_DELAY) == pdTRUE;
}

bool unlock_fb(){
    return xSemaphoreGive(fb_mutex);
}
