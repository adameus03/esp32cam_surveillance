#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_camera.h"

#include "ArduinoOTA.h"

#include "gallery.hpp"
#include "synchro.hpp"
#include "ota.hpp"

#include "WiFiCredentials.hpp"

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;


static esp_err_t rt_stream_handler(httpd_req_t *req){
    Serial.println("CLIENT RT STREAM HANDLER");
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char *part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    uint16_t fps = 0;
    ulong start_millis = millis();

    while(true){
        ArduinoOTA.handle();
        while(!lock_fb()){
            ArduinoOTA.handle();
            Serial.println("+ stream_handler lock FAIL! Delaying 100ms---");
            delay(100);
        }
        Serial.println("+ stream_handler lock successfull");

        Serial.println("Before esp_camera_fb_get() in stream handler");
        fb = esp_camera_fb_get();
        Serial.println("After esp_camera_fb_get() in stream handler");
        if (!fb) {
            Serial.println("Camera capture failed in stream handler");
            res = ESP_FAIL;
        } 

        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        ArduinoOTA.handle();
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        ArduinoOTA.handle();
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        ArduinoOTA.handle();
        //Serial.printf("Buffer length: %uB\n", (uint32_t)fb->len);
        esp_camera_fb_return(fb);
        while(!unlock_fb()){
            ArduinoOTA.handle();
            Serial.println("- stream_handler unlock FAIL! Delaying 100ms---");
            delay(100);
        }
        Serial.println("- stream_handler unlock successfull");

        ArduinoOTA.handle();

        if(res != ESP_OK){
            Serial.println("Break conditional"); ////
            break;
        }
        //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));

        fps++;
        if(millis() - start_millis >= 1000){
            Serial.printf("FPS: %u\n", fps);
            start_millis = millis();
            fps = 0;
            //saveImage(); ////
        }
    }
    return res;
}

static esp_err_t present_handler(httpd_req_t* req){
    const char* response = "PRESENT";
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

static esp_err_t past_handler(httpd_req_t* req){
    // Extract GET parameters from the query string
    char query[128];
    bool timeValueOk = false;
    int timeValue;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char time_param[32];
        if (httpd_query_key_value(query, "time", time_param, sizeof(time_param)) == ESP_OK) {
            // Handle the "time" parameter value (e.g., convert to integer)
            timeValue = atoi(time_param);
            timeValueOk = true;
            // Now you can use "timeValue" in your "/past" handler logic
        }
    }

    if(timeValueOk){
        const char* response = "TIME VALUE OK";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    else{
        const char* response = "ERROR READING TIME VALUE";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
}

void initializeWebServer(){
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    
    Serial.print("Camera Stream Ready! Go to: http://");
    Serial.print(WiFi.localIP());

    initOTA();
}

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 1U;
    ////config.server_port = 80;

    httpd_uri_t index_uri_rt_stream = {
        .uri = "/rt_stream",
        .method = HTTP_GET,
        .handler = rt_stream_handler,
        .user_ctx = NULL};

    httpd_uri_t index_uri_present = {
        .uri = "/present",
        .method = HTTP_GET,
        .handler = present_handler,
        .user_ctx = NULL};

    httpd_uri_t index_uri_past = {
        .uri = "/past",
        .method = HTTP_GET,
        .handler = past_handler,
        .user_ctx = NULL};

    ArduinoOTA.handle();

    // Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &index_uri_rt_stream);
        httpd_register_uri_handler(stream_httpd, &index_uri_present);
        httpd_register_uri_handler(stream_httpd, &index_uri_past);
    }
}