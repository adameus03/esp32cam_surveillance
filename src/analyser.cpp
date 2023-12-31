//#include "Arduino.h"
#include "analyser.hpp"

#include "esp_camera.h"

#include "EEPROM.h"

#include <stdlib.h>

#define EEPROM_SIZE 5
#define CACHING_EEPROM_LOC 0x0
#define ANAL_THRESHOLD_LOC 0x1


#define INCOMPATIBLE_BUF_SZ_DIFF_INDEX 271
#define NULL_BUF_DIFF_INDEX 0 
#define NULL_CACHED_BUF_DIFF_INDEX  272


//#define ANALYSER_APPROVAL_THRESHOLD 270

static uint32_t ANALYSER_APPROVAL_THRESHOLD = 270;

uint8_t *cached_buf = NULL;
uint32_t cached_buf_size = 0x0;
bool isCachingActivated = false;

#ifdef DIFFERENTIAL_MODE_WRT_INIT
void initAnalyser(uint8_t* buf, size_t buf_sz, size_t buf_w, size_t buf_h){

}
#else
void initAnalyser(){
    EEPROM.begin(EEPROM_SIZE);
    uint8_t eepromv_cel = EEPROM.read(CACHING_EEPROM_LOC);
    uint32_t eepromv_atl = EEPROM.readULong(ANAL_THRESHOLD_LOC);
    Serial.println("Restoring isCachingActivated=" + String(eepromv_cel));
    isCachingActivated = eepromv_cel;
    Serial.println("Restoring ANALYSER_APPROVAL_THRESHOLD=" + String(eepromv_atl));
    ANALYSER_APPROVAL_THRESHOLD = eepromv_atl;
}
#endif

void activateCaching(uint8_t* buf, size_t buf_sz){
    
    if(!isCachingActivated){
        
        isCachingActivated = true;
        EEPROM.write(CACHING_EEPROM_LOC, 0x1);
        EEPROM.commit();
        Serial.println("Caching activated");

        if(buf){
            cached_buf = (uint8_t*)malloc(buf_sz);
            memcpy(cached_buf, buf, buf_sz);
            cached_buf_size = buf_sz;
        }
        else {
            cached_buf = (uint8_t *)malloc(0x1);
            cached_buf_size = 0x1;
        }

        
    }
}

void deactivateCaching(){
    isCachingActivated = false;
    EEPROM.write(CACHING_EEPROM_LOC, 0x0);
    EEPROM.commit();
    Serial.println("Caching deactivated");
}

bool checkCaching(){
    return isCachingActivated;
}


#define CHANNEL_DIFF_MINIMUM_MODE
//#define PREWITT_MODE



uint32_t diff_index(uint8_t* jpg_buf, size_t jpg_buf_sz, size_t buf_w, size_t buf_h){
    const size_t buf_sz = buf_w * buf_h * 3;

    if(cached_buf_size != buf_sz){
        Serial.println("BUFFER SIZE DIFFERENCE");//
    }



    uint8_t* buf = (uint8_t*)malloc(buf_sz);
    jpg2rgb565(jpg_buf, jpg_buf_sz, buf, JPG_SCALE_NONE); //@@

    uint64_t ssum = 0x0;
    if(!buf){
        ssum = NULL_BUF_DIFF_INDEX;
        return ssum;
    }
    else if(!cached_buf){
        ssum = NULL_CACHED_BUF_DIFF_INDEX;
        cached_buf = (uint8_t*)realloc(cached_buf, buf_sz);
        memcpy(cached_buf, buf, buf_sz);
        cached_buf_size = buf_sz;
        return ssum;
    }
    else if(buf_sz != cached_buf_size){
        //Serial.println(cached_buf_sz)
        ssum = INCOMPATIBLE_BUF_SZ_DIFF_INDEX;
        cached_buf = (uint8_t*)realloc(cached_buf, buf_sz);
        memcpy(cached_buf, buf, buf_sz);
        cached_buf_size = buf_sz;
        return ssum;
    }
    else {
        /*
            If colsor stripes affect this, calculate the minimum ssum across the RGB channels 
        */
        uint32_t ssum = 0x0;
        /*uint8_t val1;
        uint8_t val2;
        uint8_t val3;*/

        /*for (uint32_t i = 0x0; i < buf_sz/3; i+=3){
            val1 = (buf[i] - cached_buf[i]) * (buf[i] - cached_buf[i]);
            val2 = (buf[i + 1] - cached_buf[i + 1]) * (buf[i + 1] - cached_buf[i + 1]);
            val3 = (buf[i + 2] - cached_buf[i + 2]) * (buf[i + 2] - cached_buf[i + 2]);

            ssum += std::min({val1, val2, val3});
        }*/

        uint32_t buf_3w = 3 * buf_w;
        //uint32_t buf_h = buf_sz / buf_3w;

        uint32_t temp1;
        uint32_t temp2;


        #ifdef CHANNEL_DIFF_MINIMUM_MODE
        static uint32_t channels_delta[0x3];
        #endif

        #ifndef PREWITT_MODE
        const uint8_t margin = 0x1;
        #else
        const uint8_t margin = 0x2;
        #endif

        for (uint32_t i = margin; i < buf_h - margin; i++){
            for (uint32_t j = margin; j < buf_w - margin; j++){
                for (uint32_t k = 0x0; k < 0x3; k++){
                    #ifndef PREWITT_MODE
                    temp1 = buf[i * buf_3w + 3*j+k] +
                        buf[(i - 1) * buf_3w + 3*j+k] +
                        buf[i * buf_3w + 3*(j - 1)+k] +
                        buf[(i - 1) * buf_3w + 3*(j - 1)+k] +
                        buf[(i + 1) * buf_3w + 3*j+k] +
                        buf[i * buf_3w + 3*(j + 1)+k] +
                        buf[(i + 1) * buf_3w + 3*(j + 1)+k] +
                        buf[(i - 1) * buf_3w + 3*(j + 1)+k] +
                        buf[(i + 1) * buf_3w + 3*(j - 1)+k];

                    temp2 = cached_buf[i * buf_3w + 3*j+k] +
                        cached_buf[(i - 1) * buf_3w + 3*j+k] +
                        cached_buf[i * buf_3w + 3*(j - 1)+k] +
                        cached_buf[(i - 1) * buf_3w + 3*(j - 1)+k] +
                        cached_buf[(i + 1) * buf_3w + 3*j+k] +
                        cached_buf[i * buf_3w + 3*(j + 1)+k] +
                        cached_buf[(i + 1) * buf_3w + 3*(j + 1)+k] +
                        cached_buf[(i - 1) * buf_3w + 3*(j + 1)+k] +
                        cached_buf[(i + 1) * buf_3w + 3*(j - 1)+k];

                    #else
                    temp1 = buf[(i-2)*buf_3w+3*(j+1)+k]+
                            buf[(i-2)*buf_3w+3*(j+2)+k]+
                            2*buf[(i-1)*buf_3w+3*(j+1)+k]+
                            2*buf[(i-1)*buf_3w+3*(j+2)+k]+
                            3*buf[i*buf_3w+3*(j+1)+k]+
                            3*buf[i*buf_3w+3*(j+2)+k]+
                            2*buf[(i+1)*buf_3w+3*(j+1)+k]+
                            2*buf[(i+1)*buf_3w+3*(j+2)+k]+
                            buf[(i+2)*buf_3w+3*(j+1)+k]+
                            buf[(i+2)*buf_3w+3*(j+2)+k]-

                            buf[(i-2)*buf_3w+3*(j-1)+k]-
                            buf[(i-2)*buf_3w+3*(j-2)+k]-
                            2*buf[(i-1)*buf_3w+3*(j-1)+k]-
                            2*buf[(i-1)*buf_3w+3*(j-2)+k]-
                            3*buf[i*buf_3w+3*(j-1)+k]-
                            3*buf[i*buf_3w+3*(j-2)+k]-
                            2*buf[(i+1)*buf_3w+3*(j-1)+k]-
                            2*buf[(i+1)*buf_3w+3*(j-2)+k]-
                            buf[(i+2)*buf_3w+3*(j-1)+k]-
                            buf[(i+2)*buf_3w+3*(j-2)+k];

                    temp2 = cached_buf[(i-2)*buf_3w+3*(j+1)+k]+
                            cached_buf[(i-2)*buf_3w+3*(j+2)+k]+
                            2*cached_buf[(i-1)*buf_3w+3*(j+1)+k]+
                            2*cached_buf[(i-1)*buf_3w+3*(j+2)+k]+
                            3*cached_buf[i*buf_3w+3*(j+1)+k]+
                            3*cached_buf[i*buf_3w+3*(j+2)+k]+
                            2*cached_buf[(i+1)*buf_3w+3*(j+1)+k]+
                            2*cached_buf[(i+1)*buf_3w+3*(j+2)+k]+
                            cached_buf[(i+2)*buf_3w+3*(j+1)+k]+
                            cached_buf[(i+2)*buf_3w+3*(j+2)+k]-

                            cached_buf[(i-2)*buf_3w+3*(j-1)+k]-
                            cached_buf[(i-2)*buf_3w+3*(j-2)+k]-
                            2*cached_buf[(i-1)*buf_3w+3*(j-1)+k]-
                            2*cached_buf[(i-1)*buf_3w+3*(j-2)+k]-
                            3*cached_buf[i*buf_3w+3*(j-1)+k]-
                            3*cached_buf[i*buf_3w+3*(j-2)+k]-
                            2*cached_buf[(i+1)*buf_3w+3*(j-1)+k]-
                            2*cached_buf[(i+1)*buf_3w+3*(j-2)+k]-
                            cached_buf[(i+2)*buf_3w+3*(j-1)+k]-
                            cached_buf[(i+2)*buf_3w+3*(j-2)+k];

                    #endif

                    temp1 /= 9;
                    temp2 /= 9;

                    #ifndef CHANNEL_DIFF_MINIMUM_MODE
                    ssum += (temp1 - temp2) * (temp1 - temp2);
                    #else 
                    channels_delta[k] = (temp1 - temp2) * (temp1 - temp2);
                    #endif
                }
                #ifdef CHANNEL_DIFF_MINIMUM_MODE
                //ssum += std::min_element(channels_delta, channels_delta+0x3);
                if(channels_delta[0x0] < channels_delta[0x1]){
                    if(channels_delta[0x0] < channels_delta[0x2]){
                        //channels 0 delta is min
                        ssum += channels_delta[0x0];
                    }
                    else {
                        //channels 2 delta is min
                        ssum += channels_delta[0x2];
                    }
                }
                else {
                    if(channels_delta[0x1] < channels_delta[0x2]){
                        //channels 1 delta is min
                        ssum += channels_delta[0x1];
                    }
                    else {
                        //channels 2 delta is min
                        ssum += channels_delta[0x2];
                    }
                }
                #endif
            }
        }
        
        memcpy(cached_buf, buf, buf_sz);
        free(buf); //@@
        return ssum / buf_sz;
    }
}

uint32_t checkApproval(uint8_t* buf, size_t buf_sz, size_t buf_w, size_t buf_h){
    uint32_t diff = diff_index(buf, buf_sz, buf_w, buf_h);
    Serial.println(String(diff) + "/ " + String(ANALYSER_APPROVAL_THRESHOLD));
    if(isCachingActivated){
        return diff >= ANALYSER_APPROVAL_THRESHOLD;
    }
    else {
        return true;
    }
}

const uint32_t getAnalyserApprovalThreshold(){
    return ANALYSER_APPROVAL_THRESHOLD;
}

void setAnalyserApprovalThreshold(const uint32_t& val){
    EEPROM.writeULong(ANAL_THRESHOLD_LOC, val);
    EEPROM.commit();
    ANALYSER_APPROVAL_THRESHOLD = val;
}


