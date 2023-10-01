#include "Arduino.h"

///<differential analyser select option>
#define DIFFERENTIAL_MODE_WRT_PREV
//#define DIFFERENTIAL_MODE_WRT_INIT
///</differential analyser select option>

#ifdef DIFFERENTIAL_MODE_WRT_INIT
void initAnalyser(uint8_t* buf, size_t buf_sz, size_t buf_w, size_t buf_h);
#else
void initAnalyser();
#endif

void activateCaching(uint8_t *buf, size_t buf_sz);

void deactivateCaching();

bool checkCaching();

//uint32_t diff_index(uint8_t *buf, size_t buf_sz);
uint32_t diff_index(uint8_t *jpg_buf, size_t jpg_buf_sz, size_t buf_w, size_t buf_h);

uint32_t checkApproval(uint8_t *buf, size_t buf_sz, size_t buf_w, size_t buf_h);

const uint32_t getAnalyserApprovalThreshold();

void setAnalyserApprovalThreshold(const uint32_t &val);