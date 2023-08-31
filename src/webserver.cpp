#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_camera.h"

#include "gallery.hpp"
#include "ota.hpp" // for public OTA toggler
//#include "synchro.hpp"

#include "confidential/WiFiCredentials.hpp"

#include "fsutils.hpp"

#include "time.h"

#include "analyser.hpp"

#define PART_BOUNDARY "123456789000000000000987654321"

#define MAX_BLANK 15

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
    //@@deactivateStandaloneImageSaveCycle();

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

        ///<compression>
        /*uint8_t *jpg_buf = NULL;
        size_t jpg_buf_len;
        bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
        if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
        }*/
        ///</compression>
        
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
            //res = httpd_resp_send_chunk(req, (char*)jpg_buf, jpg_buf_len);
        }

        ///<compression free>
        //free(jpg_buf);
        ///</compression free>
        
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

    //@@reactivateStandaloneImageSaveCycle(); //RE!
    return res; //?
}

static esp_err_t stream_handler(httpd_req_t* req){
    const char *response =
#include "htdocs/stream_wrapper.html"
    ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/*static esp_err_t present_handler(httpd_req_t* req){
    const char* response = "PRESENT";
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}*/

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

    httpd_resp_send_chunk(req, NULL, 0);

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

static esp_err_t send_list_chunk(httpd_req_t* req, const uint32_t& offset, const uint32_t& chunk_len, std::vector<String>* filePaths){
    String response = "";
    //Serial.println("LIST SIZE: " + String(filePaths->size())); // DEBUG
    for (uint32_t i = 0x0; i < chunk_len; i++){
        //response += "<p>" + (*filePaths)[offset+i] + "</p>";
        //response += "<p><a href='/pastselect?pass=/past?nav=atindex&ind=" + String(i) + "'>" + (*filePaths)[offset + i] + "</a></p>";
        response += "<p><a href='/pastselect?pass=%2Fpast%3Fnav%3Datindex%26ind%3D" + String(i) + "'>" + (*filePaths)[offset + i] + "</a></p>";
        
    }
    return httpd_resp_send_chunk(req, response.c_str(), response.length());
}

static esp_err_t past_index_subhandler(httpd_req_t* req, char* query){
    char nav_param[20];
    if (httpd_query_key_value(query, "nav", nav_param, sizeof(nav_param)) != ESP_OK) {
        const char *response = "No time & index param or error reading time & index param.";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    if(!getCurrImageName()){
        /*const char *response = "No base image selected!";
        httpd_resp_send(req, response, strlen(response));*/
        rewindGallery(); //by default set the gallery index as 0
        //return ESP_OK;
    }   
    
    if(!strcmp("datetime", nav_param)){
        const char *response = getCurrImageName();
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    else if(!strcmp("list", nav_param)){
        /*String response = "";
        std::vector<String>* filePaths = getMasterEntries();
        Serial.println("LIST SIZE: " + String(filePaths->size())); // DEBUG
        for (uint32_t i = 0x0; i < filePaths->size(); i++){
            response += "<p>" + (*filePaths)[i] + "</p>";
        }
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;*/
        std::vector<String> *filePaths = getMasterEntries();
        Serial.println("LIST SIZE: " + String(filePaths->size())); // DEBUG
        uint32_t chunk_len = 100;
        uint32_t full_chunks_count = filePaths->size() / chunk_len;
        uint32_t chunk_rem = filePaths->size() % chunk_len;

        uint32_t offset = 0x0;

        for (uint32_t i = 0x0; i < full_chunks_count; i++){
            send_list_chunk(req, offset, chunk_len, filePaths);
            offset += chunk_len;
        }
        if(chunk_rem){
            send_list_chunk(req, offset, chunk_rem, filePaths);
        }
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }
    else {
        File file;
        esp_err_t res = ESP_OK;
        if(!strcmp("next", nav_param)){
            file = getNextImage();
        }
        else if(!strcmp("prev", nav_param)){
            file = getPrevImage();
        }
        else if(!strcmp("first", nav_param)){
            file = getFirstHistoryImage();
        }
        else if(!strcmp("last", nav_param)){
            file = getLastHistoryImage();
        }
        else if(!strcmp("atindex", nav_param)){
            char ind_param[20];
            if (httpd_query_key_value(query, "ind", ind_param, sizeof(ind_param)) != ESP_OK) {
                const char *response = "No ind param.";
                httpd_resp_send(req, response, strlen(response));
                return ESP_OK;
            }
            file = getImage(atoll(ind_param));
        }
        else {
            const char *response = "Invalid nav option selected";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }

        if(!file){
            const char *response = "NULL file";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }

        Serial.println("READING file, name: " + String(file.name()));

        static size_t fsize;
        fsize = file.size();

        char *part_buf[64];
        //res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
        

        
        res = httpd_resp_set_type(req, "image/jpeg");

        if (res != ESP_OK)
        {
            return res;
        }

        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fsize);
            //res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            char hlen_str[25];
            ulltoa(hlen, hlen_str, 10);
            res = httpd_resp_set_hdr(req, "Content-Length", hlen_str);
        }

        if(res == ESP_OK){
            //res = httpd_resp_send_chunk(req, (const char *)buffer, buffer_len);
            res = stream_image(std::make_shared<File>(file), req, fsize);
        }
        /*if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }*/

        file.close();

        if(res != ESP_OK){
            Serial.println("Break conditional(past)");
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    
}

static esp_err_t action_past_handler(httpd_req_t* req){
    char query[128];
    char base_param[32];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "base", base_param, sizeof(base_param)) != ESP_OK) {
            /*const char *response = "No time param or error reading time param.";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;*/
            return past_index_subhandler(req, query); //@@ What if ESP_FAIL?
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
    if(!strptime(base_param, "%Y-%m-%d_%H-%M-%S", &timeinfo)){
        const char *response = "Incorrect datetime format.";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    uint32_t blank_count = 0;

    //obtain first file after the given search datetime
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

        file = SD_MMC.open(path.c_str(), FILE_READ);
        if(file){
            break;
        }
        blank_count++;
        if(blank_count > MAX_BLANK){
            const char *response = "<h1>Dla tego czasu nie znaleziono klatek.</h1>";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }

        //"increment" timeParam(year,month...second)
        liu(&little_index, &timeinfo);
    }

    setBaseImage(file);

    /*esp_err_t res = ESP_OK;
    char *part_buf[64];
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    while(file){
        Serial.println("READING file " + path);

        static size_t fsize;
        fsize = file.size();

        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fsize);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }

        if(res == ESP_OK){
            res = stream_image(std::make_shared<File>(file), req, fsize);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }


        file.close();
        if(res != ESP_OK){
            Serial.println("Break conditional(past)");
            break;
        }
        file = file.openNextFile();
    }*/

    Serial.println("READING file, name: " + String(file.name()));

    esp_err_t res = ESP_OK;
    static size_t fsize;
    fsize = file.size();

    char *part_buf[64];
    res = httpd_resp_set_type(req, "image/jpeg");

    if(res == ESP_OK){
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fsize);
        //res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        char hlen_str[25];
        ulltoa(hlen, hlen_str, 10);
        res = httpd_resp_set_hdr(req, "Content-Length", hlen_str);
    }

    if(res == ESP_OK){
        //res = httpd_resp_send_chunk(req, (const char *)buffer, buffer_len);
        res = stream_image(std::make_shared<File>(file), req, fsize);
    }

    file.close(); //@@

    /*const char *response = "Base image set.";
    httpd_resp_send(req, response, strlen(response));*/
    //httpd_sess_trigger_close
    return ESP_OK;
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

    uint32_t blank_count = 0;
    esp_err_t res = ESP_OK;
    char *part_buf[64];
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }
    
    ulong start_millis = millis();
    //@@deactivateStandaloneImageSaveCycle();

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

    //@@reactivateStandaloneImageSaveCycle();
    return res; //?
}

static esp_err_t root_handler(httpd_req_t* req){
    const char *response =
#include "htdocs/menu.html"
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
    #include "htdocs/pastselect.html"
    ;
    //const char *response = "placeholder";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static esp_err_t advanced_handler(httpd_req_t* req){
    const char *response =
#include "htdocs/advanced.html"
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
#include "htdocs/publicOTAToggle.html"
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

static esp_err_t threshold_subhandler(httpd_req_t* req, char* query){
    char val_param[20];
    if (httpd_query_key_value(query, "val", val_param, sizeof(val_param)) != ESP_OK) {
        //const char *response = "No val param or error reading op param.";
        String response =
#include "htdocs/threshold.html"
        ;

        response.replace("%{VAL}%", String(getAnalyserApprovalThreshold()));
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;
    }
    else {
        uint32_t val = atoi(val_param);
        setAnalyserApprovalThreshold(val);
        const char *response = "Successfully set the new threshold value";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
}

static esp_err_t cmd_handler(httpd_req_t* req){
    char query[128];
    char op_param[10];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "op", op_param, sizeof(op_param)) != ESP_OK) {
            const char *response = "No op param or error reading op param.";
            httpd_resp_send(req, response, strlen(response));
            return ESP_OK;
        }
    }
    else {
        //const char *response = "Invalid query.";
        const char *response = "Nothing to do";
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    if(!strcmp("format", op_param)){
        //Formatuj {{{implement ???}}}


        const char *response = "Micro SD format not implemented";
        httpd_resp_send(req, response, strlen(response));
    }
    else if(!strcmp("diff", op_param)){
        if(checkCaching()){
            deactivateCaching();
            const char *response = "Deactivated.";
            httpd_resp_send(req, response, strlen(response));
        }
        else {
            activateCaching(NULL, 0x0);
            const char *response = "Activated.";
            httpd_resp_send(req, response, strlen(response));
        }
    }
    else if(!strcmp("threshold", op_param)){
        threshold_subhandler(req, query);
    }
    else {
        const char *response = "Wrong op value provided.";
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

    httpd_uri_t index_uri_past = {
        .uri = "/past",
        .method = HTTP_GET,
        .handler = /*past_handler*/action_past_handler,
        .user_ctx = NULL
    };

    httpd_uri_t index_uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
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

    httpd_uri_t index_uri_cmd = {
        .uri = "/cmd",
        .method = HTTP_GET,
        .handler = cmd_handler,
        .user_ctx = NULL
    };

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

        httpd_register_uri_handler(server_httpd, &index_uri_cmd);

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