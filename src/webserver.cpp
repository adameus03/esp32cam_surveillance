#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_camera.h"

#include "gallery.hpp"
#include "ota.hpp" // for public OTA toggler
//#include "synchro.hpp"

#include "WiFiCredentials.hpp"

#include "fsutils.hpp"

#include "time.h"

#define PART_BOUNDARY "123456789000000000000987654321"

#define MAX_BLANK 10

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t server_httpd = NULL;


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

    Serial.println("Deattaching the image save interrupt");
    deactivateStandaloneImageSaveCycle();

    //uint16_t fps = 0;
    ulong start_millis = millis();

    while(true){
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
        
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        if((millis() - start_millis)*1000 >= getImageIntervalUs()){//optimize?
            //T//saveImage(); <-- HERE
            start_millis = millis();
        }
        
        //Serial.printf("Buffer length: %uB\n", (uint32_t)fb->len);
        esp_camera_fb_return(fb);

        if(res != ESP_OK){
            Serial.println("Break conditional. Reattaching the image save interrupt"); ////
            break;
        }
        //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));

        //fps++;
        /*if(millis() - start_millis >= 1000){
            Serial.printf("FPS: %u\n", fps);
            start_millis = millis();
            fps = 0;
            //saveImage(); ////
        }*/
    }

    reactivateStandaloneImageSaveCycle(); //RE!
    return res; //?
}

static esp_err_t stream_handler(httpd_req_t* req){
    const char *response =
#include "stream_wrapper.h"
    ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static esp_err_t present_handler(httpd_req_t* req){
    const char* response = "PRESENT";
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

esp_err_t stream_image(std::shared_ptr<File> file, httpd_req_t* req, const size_t& fsize){
    // Read and send image data in chunks
    esp_err_t res = ESP_OK;
    const size_t chunkSize = 1024; // Adjust this as needed
    uint8_t buffer[chunkSize];
    size_t remainingBytes = fsize;
    while (remainingBytes > 0) {
        static size_t readBytes;
        readBytes = file->read(buffer, std::min(chunkSize, remainingBytes));
        if (readBytes > 0) {
            res = httpd_resp_send_chunk(req, (const char *)buffer, readBytes);
            if (res != ESP_OK) {
                break;
            }
            remainingBytes -= readBytes;
        } else {
            break;
        }
    }
    return res;
}

void liu(uint8_t* lip, tm* tip){//little index update
    if(*lip>=getImageMaxLittleIndex()){
        time_t timestamp = mktime(tip);
        timestamp++;
        *tip = *localtime(&timestamp);
        *lip = 0;
    }
    else {
        (*lip)++;
    }
}

static esp_err_t past_handler(httpd_req_t* req){
    // Extract GET parameters from the query string
    char query[128];
    char time_param[32];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "time", time_param, sizeof(time_param)) != ESP_OK) {
            const char *response = "No time param or error reading time param.";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }
    }
    else {
        const char *response = "Invalid query.";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    String path;// = time_param;
    File file;

    tm timeinfo;
    uint8_t little_index = 0;

    //parse time_param
    if(!strptime(time_param, "%Y-%m-%d_%H-%M-%S", &timeinfo)){
        const char *response = "Incorrect datetime format.";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    //////
    //////
    Serial.println("IM HERE");
    /*const char *response = "bingo";
    httpd_resp_send(req, response, strlen(response));
    reactivateStandaloneImageSaveCycle();
    return ESP_OK;*/
    ///////
    /////

    uint8_t blank_count = 0;
    esp_err_t res = ESP_OK;
    char *part_buf[64];
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }
    
    ulong start_millis = millis();
    deactivateStandaloneImageSaveCycle();

    while(true)
    {
        //[combime year,month...second into time_param]
        char datetime_pli[24];
        strftime(datetime_pli, sizeof(datetime_pli), "%Y-%m-%d_%H-%M-%S", &timeinfo);
        //itoa()
        //strcpy(datetime_pli + 19, "_" + itoa(little_index));
        sprintf(datetime_pli + strlen(datetime_pli), "_%u", little_index);

        path = datetime_pli; //instead of time_param
        path = "/frame_" + path + ".jpg";

        Serial.println(datetime_pli);
        Serial.println("_" + little_index);
        Serial.println("TRYREAD file " + path + " ...");

        file = SD_MMC.open(path.c_str(), FILE_READ); //{{{OPEN NEXT FILE?}}}
        if(file){
            Serial.println("READING file " + path);
            blank_count = 0;

            static size_t fsize;
            fsize = file.size();

            /*static ulong tdiff_us;
            static ulong rem_us;
            tdiff_us = 1000*(millis() - start_millis);
            rem_us = getImageIntervalUs() - tdiff_us;
            if(rem_us>=1000){
                delay(rem_us / 1000);
            }*/
            

            if(res == ESP_OK){
                size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fsize);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }

            if(res == ESP_OK){
                //res = httpd_resp_send_chunk(req, (const char *)buffer, buffer_len);
                res = stream_image(std::make_shared<File>(file), req, fsize);
            }
            if(res == ESP_OK){
                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            }


            /*if(tdiff_us >= getImageIntervalUs()){//optimize?
                //T//saveImage(); <-- HERE (TOO)
                start_millis = millis();
            }*/
            start_millis = millis();

            if(res != ESP_OK){
                Serial.println("Break conditional(past)");
                break;
            }

            /*little_index++;
            if(little_index == getImageMaxLittleIndex()){

            }*/
            liu(&little_index, &timeinfo);
            continue;
        }

        blank_count++;
        if(blank_count > MAX_BLANK){
            const char *response = "<h1>Osiągnięto koniec odtwarzania</h1>";
            httpd_resp_send(req, response, strlen(response));
            break;
        }

        //"increment" timeParam(year,month...second)
        liu(&little_index, &timeinfo);
    }

    reactivateStandaloneImageSaveCycle();
    return res; //?
}

static esp_err_t root_handler(httpd_req_t* req){
    const char *response =
#include "menu.h"
        ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    

    return ESP_OK;
}

static esp_err_t ls_handler(httpd_req_t* req){
    String response = "";

    char query[128];
    char root_name[32];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (!httpd_query_key_value(query, "d", root_name, sizeof(root_name)) == ESP_OK) {
            strcpy(root_name, "/");
        }
    }
    else {
        strcpy(root_name, "/");
    }

    listFiles(root_name, &response);

    Serial.println("************\n************\n***************"); //
    Serial.println(response);                                      //
    Serial.println("************\n************\n***************"); //
    httpd_resp_send(req, response.c_str(), response.length());

    return ESP_OK;
}

static esp_err_t pastselect_handler(httpd_req_t* req){
    //send pastselect website]

    const char *response = 
    #include "pastselect.h"
    ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static esp_err_t advanced_handler(httpd_req_t* req){
    const char *response =
#include "advanced.h"
        ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/* Non-local OTA */
static esp_err_t ota_upload_handler(httpd_req_t* req){
    //httpd
    /*const char *response = "Hello OTA. Not implemented";
    httpd_resp_send(req, response, strlen(response));*/


    char query[128];
    char state_param[4];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "state", state_param, sizeof(state_param)) != ESP_OK) {
            const char *response = "No state param or error reading state param.";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }
    }
    else {
        //const char *response = "Invalid query.";
        const char *response =
#include "publicOTAToggle.h"
            ;
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    if(!strcmp("on", state_param)){
        resumePublicOTA();
        const char *response = "PublicOTA is on";
        httpd_resp_send(req, response, strlen(response));
    }
    else if(!strcmp("off", state_param)){
        haltPublicOTA();
        const char *response = "PublicOTA is off";
        httpd_resp_send(req, response, strlen(response));
    }   
    else {
        const char *response = "Wrong state value provided.";
        httpd_resp_send(req, response, strlen(response));
    }
    return ESP_OK; //{{{return res instead?}}}
}

void initializeWebServer(){
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    
    Serial.print("Server local IP: ");
    Serial.print(WiFi.localIP());

    //initOTA();
}

/*esp_err_t wildcard_level_1_uri_handler_router(httpd_req_t* req){
    const char *uri = req->uri;
    if(!strcmp("m/rt_stream", uri)){
        return rt_stream_handler(req);
    }
    else if(!strcmp("/m/stream", uri)){
        return stream_handler(req);
    }
    else if(!strcmp("/m/present", uri)){
        return present_handler(req);
    }
    else if(!strcmp("/m/past", uri)){
        return past_handler(req);
    }
    else if(!strcmp("/m/", uri)){
        return root_handler(req);
    }
    else if(!strcmp("/m/ls", uri)){
        return ls_handler(req);
    }
    else if(!strcmp("/m/pastselect", uri)){
        return pastselect_handler(req);
    }
    else if(!strcmp("/m/advanced", uri)){
        return advanced_handler(req);
    }
    else if(!strcmp("/m/ota_upload", uri)){
        return ota_upload_handler(req);
    }
    //httpd_resp_send_404(req);
    httpd_resp_send(req, "Hello", 5);
    return ESP_OK;
}*/

void startWebServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 1U;
    ////config.server_port = 80;

    httpd_uri_t index_uri_rt_stream = {
        .uri = "/rt_stream",
        .method = HTTP_GET,
        .handler = rt_stream_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_stream = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_present = {
        .uri = "/present",
        .method = HTTP_GET,
        .handler = present_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_past = {
        .uri = "/past",
        .method = HTTP_GET,
        .handler = past_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_ls = {
        .uri = "/ls",
        .method = HTTP_GET,
        .handler = ls_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_pastselect = {
        .uri = "/pastselect",
        .method = HTTP_GET,
        .handler = pastselect_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_advanced = {
        .uri = "/advanced",
        .method = HTTP_GET,
        .handler = advanced_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_ota_upload = {
        .uri = "/ota_upload",
        .method = HTTP_GET,
        .handler = ota_upload_handler,
        .user_ctx = NULL
    };
    /*httpd_uri_t index_uri_wildcard_lvl_1 = {
        .uri = "/m/*",
        .method = HTTP_GET,
        .handler = wildcard_level_1_uri_handler_router,
        .user_ctx = NULL};*/

    // Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&server_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server_httpd, &index_uri_rt_stream);
        httpd_register_uri_handler(server_httpd, &index_uri_stream);
        //httpd_register_uri_handler(server_httpd, &index_uri_present);
        httpd_register_uri_handler(server_httpd, &index_uri_past);
        httpd_register_uri_handler(server_httpd, &index_uri_root);
        //httpd_register_uri_handler(server_httpd, &index_uri_ls);
        httpd_register_uri_handler(server_httpd, &index_uri_pastselect);
        httpd_register_uri_handler(server_httpd, &index_uri_advanced);
        httpd_register_uri_handler(server_httpd, &index_uri_ota_upload);

        /*esp_err_t res = httpd_unregister_uri(server_httpd, "*");
        if(res != ESP_OK){
            Serial.println("Failed to unregister handlers for /* !");
            Serial.println("Reason: ");
            Serial.println(esp_err_to_name(res));
        }*/
        /*esp_err_t res = httpd_register_uri_handler(server_httpd, &index_uri_wildcard_lvl_1);
        if(res != ESP_OK){
            Serial.println("Failed to register the uri handler for /m/* !");
            Serial.println("Reason: ");
            Serial.println(esp_err_to_name(res));
        };*/
    }
}