#include <HTTPClient.h>
#include "device_specific.h"

//#include <ESPAsyncWebServer.h>

//AsyncWebServer androidServer(80);
//AsyncEventSource androidEvents("/events");

//const char* speakerServerPath = "http://192.168.1.254/cmd?op=jingle";
const char* speakerServerPath = "http://178.183.205.144:8090/cmd?op=jingle&cid=";
HTTPClient http;

/*void initializeAlarmAndroidServer(){
    androidEvents.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 10000);
    });
    androidServer.addHandler(&androidEvents);
    androidServer.begin();
}*/

void notifyAlarmSoundSystem(){
    http.begin(speakerServerPath + String(CAM_ID));
    http.GET();
    http.end();
    //androidEvents.send("Message", "Event", millis());
}