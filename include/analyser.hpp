#include "Arduino.h"

void activateCaching(uint8_t *buf, size_t buf_sz);

void deactivateCaching();

bool checkCaching();

//uint32_t diff_index(uint8_t *buf, size_t buf_sz);
uint32_t diff_index(uint8_t *jpg_buf, size_t jpg_buf_sz, size_t buf_w, size_t buf_h);

uint32_t checkApproval(uint8_t *buf, size_t buf_sz, size_t buf_w, size_t buf_h);

const uint32_t getAnalyserApprovalThreshold();

void setAnalyserApprovalThreshold(const uint32_t &val);