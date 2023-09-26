#include <WiFi.h>
#include "esp_http_server.h"
//$$#include "esp_https_server.h"
#include "esp_camera.h"

#include "analyser.hpp"

void initializeWebServer();
void startWebServer();

/*void HTTPTickImplied();//
bool checkIfTimeForHTTPHandle();//*/