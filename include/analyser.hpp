#include "Arduino.h"

void activateCaching(uint8_t *buf, size_t buf_sz);

void deactivateCaching();

bool checkCaching();

uint32_t diff_index(uint8_t *buf, size_t buf_sz);

uint32_t checkApproval(uint8_t *buf, size_t buf_sz, size_t buf_w);