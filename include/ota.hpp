#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

#include "WebServer.h" // for non-local OTA
#include "Update.h"    //also for non-local OTA

#include "confidential/OTACredentials.hpp"

void initOTA();
void OTATickImplied();
void startHandlingOTA();
bool checkIfTimeForOTAHandle();

void publicOTATickImplied(); //
void onPublicOTATimer(); //
void startHandlingPublicOTA(); //
bool checkIfTimeForPublicOTAHandle(); //

void haltPublicOTA();
void resumePublicOTA();