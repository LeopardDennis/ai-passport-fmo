#include "fmo_provision.h"
#include "fmo_credentials.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdatomic.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct { char ssid[33]; char password[64]; } credentials_t;
static QueueHandle_t s_pending;
static atomic_int s_status; /* 0 ready, 1 testing, 2 failed, 3 saved, 4 storage error */
static char s_token[33];
static uint32_t s_ap_address;
static wifi_ap_record_t s_records[16];
static uint16_t s_record_count;
static const char page[] =
"<!doctype html><html lang='zh-CN'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>FMO Wi-Fi 配网</title><style>body{background:#101010;color:#eee;font:16px system-ui;max-width:440px;margin:32px auto;padding:20px}"
"h1{color:#ff8a00}input,select,button{box-sizing:border-box;width:100%;padding:14px;margin:8px 0 18px;font:inherit}button{background:#ff8a00;border:0}small{color:#bbb}</style>"
"<h1>FMO · Wi-Fi 配网</h1><p>连接成功后自动寻找 fmo.local</p>"
"<form id='form'><label for='net'>附近的 2.4 GHz Wi-Fi</label><select id='net'><option value=''>手动输入 / 隐藏网络</option></select>"
"<label for='ssid'>Wi-Fi 名称</label><input id='ssid' required autocomplete='off'>"
"<label for='pass'>Wi-Fi 密码</label><input id='pass' type='password' autocomplete='new-password'>"
"<small>开放网络可留空；加密网络密码为 8–63 字节。</small><button id='save'>连接并保存</button></form>"
"<p id='result' role='status'></p><script>const token='";
static const char page_end[] =
"';const form=document.getElementById('form'),ssid=document.getElementById('ssid'),pass=document.getElementById('pass'),net=document.getElementById('net'),save=document.getElementById('save'),result=document.getElementById('result');"
"fetch('/networks').then(r=>r.json()).then(a=>a.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=n;net.append(o)})).catch(()=>{});"
"net.onchange=()=>{if(net.value)ssid.value=net.value};"
"async function poll(){try{const r=await fetch('/status',{cache:'no-store'});const s=await r.json();"
"if(s===3){result.textContent='配网成功，热点即将关闭。设备正在连接 FMO。';pass.value='';return}"
"if(s===2||s===4){result.textContent=s===2?'连接失败，请检查密码和信号后重试。':'保存失败，旧配置未被主动删除。请重试。';save.disabled=false;return}"
"result.textContent='正在验证连接，请稍候…';setTimeout(poll,1000)}catch(e){result.textContent='请查看设备屏幕；若仍在配网，请重新连接热点并刷新。';save.disabled=false}}"
"form.onsubmit=async e=>{e.preventDefault();save.disabled=true;try{const r=await fetch('/configure',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:JSON.stringify({ssid:ssid.value,password:pass.value})});"
"if(!r.ok)throw Error();poll()}catch(e){result.textContent='提交失败，请检查输入或重新连接热点。';save.disabled=false}};</script></html>";

static bool local_request(httpd_req_t *req)
{
    struct sockaddr_in local; socklen_t size = sizeof(local);
    return getsockname(httpd_req_to_sockfd(req), (struct sockaddr *)&local, &size) == 0 &&
           local.sin_addr.s_addr == s_ap_address;
}
static esp_err_t root_get(httpd_req_t *req)
{
    if (!local_request(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AP only");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    httpd_resp_send_chunk(req, page, sizeof(page)-1);
    httpd_resp_send_chunk(req, s_token, strlen(s_token));
    httpd_resp_send_chunk(req, page_end, sizeof(page_end)-1);
    return httpd_resp_send_chunk(req, NULL, 0);
}
static esp_err_t networks_get(httpd_req_t *req)
{
    if (!local_request(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AP only");
    cJSON *array = cJSON_CreateArray();
    if (!array) return ESP_ERR_NO_MEM;
    for (unsigned i=0; i<s_record_count; ++i) {
        s_records[i].ssid[32] = 0;
        if (s_records[i].ssid[0]) cJSON_AddItemToArray(array, cJSON_CreateString((char *)s_records[i].ssid));
    }
    char *json = cJSON_PrintUnformatted(array); cJSON_Delete(array);
    if (!json) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, json); cJSON_free(json); return err;
}
static esp_err_t status_get(httpd_req_t *req)
{
    if (!local_request(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AP only");
    char text[8]; snprintf(text, sizeof(text), "%d", atomic_load(&s_status));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, text);
}
static esp_err_t configure_post(httpd_req_t *req)
{
    char token[33];
    if (!local_request(req) || httpd_req_get_hdr_value_str(req, "X-Setup-Token", token, sizeof(token)) != ESP_OK ||
        strcmp(token, s_token)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid session");
    if (req->content_len == 0 || req->content_len > 768)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid length");
    char body[769]; size_t used=0;
    int64_t deadline = esp_timer_get_time() + 10000000;
    while (used < req->content_len) {
        if (esp_timer_get_time() >= deadline) return ESP_FAIL;
        int n = httpd_req_recv(req, body+used, req->content_len-used);
        if (n <= 0) return ESP_FAIL;
        used += (size_t)n;
    }
    body[used] = 0;
    /* Reject escaped NULs: cJSON strings otherwise hide trailing bytes. */
    cJSON *json = (memchr(body, 0, used) || strstr(body, "\\u0000")) ? NULL :
        cJSON_ParseWithOpts(body, NULL, true);
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
    credentials_t value = {0};
    bool valid = cJSON_IsString(ssid) && cJSON_IsString(password) &&
        fmo_credentials_valid(ssid->valuestring, strlen(ssid->valuestring), password->valuestring, strlen(password->valuestring));
    if (valid) {strcpy(value.ssid, ssid->valuestring); strcpy(value.password, password->valuestring);}
    cJSON_Delete(json); memset(body, 0, sizeof(body));
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid credentials");
    int status = atomic_load(&s_status);
    if (status == 1 || status == 3) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Busy");
    atomic_store(&s_status, 1);
    if (xQueueSend(s_pending, &value, 0) != pdTRUE) {atomic_store(&s_status, status); return ESP_FAIL;}
    memset(&value, 0, sizeof(value));
    return httpd_resp_sendstr(req, "Accepted");
}

bool fmo_provision_load(wifi_config_t *config, bool *force)
{
    nvs_handle_t nvs; *force = false;
    if (nvs_open("fmo_wifi", NVS_READONLY, &nvs) != ESP_OK) return false;
    uint8_t flag=0; nvs_get_u8(nvs, "setup", &flag); *force = flag != 0;
    credentials_t value={0}; size_t size=sizeof(value);
    esp_err_t err=nvs_get_blob(nvs, "credentials", &value, &size); nvs_close(nvs);
    bool valid=err == ESP_OK && size == sizeof(value) && value.ssid[32] == 0 && value.password[63] == 0 &&
        fmo_credentials_valid(value.ssid, strlen(value.ssid), value.password, strlen(value.password));
    if (valid) {memset(config,0,sizeof(*config)); memcpy(config->sta.ssid,value.ssid,strlen(value.ssid)); memcpy(config->sta.password,value.password,strlen(value.password));}
    memset(&value,0,sizeof(value)); return valid;
}
esp_err_t fmo_provision_force(void)
{
    nvs_handle_t nvs; esp_err_t err=nvs_open("fmo_wifi",NVS_READWRITE,&nvs);
    if (err != ESP_OK) return err;
    err=nvs_set_u8(nvs,"setup",1); if(err == ESP_OK)err=nvs_commit(nvs); nvs_close(nvs); return err;
}
esp_err_t fmo_provision_run(EventGroupHandle_t bits, EventBits_t ready, fmo_setup_display_t display)
{
    s_pending=xQueueCreate(1,sizeof(credentials_t)); if(!s_pending)return ESP_ERR_NO_MEM;
    esp_netif_t *ap=esp_netif_create_default_wifi_ap();
    if(!ap){vQueueDelete(s_pending);return ESP_ERR_NO_MEM;}
    httpd_handle_t server=NULL;
    uint8_t mac[6] = {0};
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(ap);
        vQueueDelete(s_pending);
        s_pending = NULL;
        return err;
    }
    wifi_config_t config={0};
    snprintf((char *)config.ap.ssid,sizeof(config.ap.ssid),"FMO-Setup-%02X%02X",mac[4],mac[5]);
    snprintf((char *)config.ap.password,sizeof(config.ap.password),"%08" PRIX32 "%04" PRIX32,
             esp_random(), esp_random() & 0xffff);
    config.ap.authmode=WIFI_AUTH_WPA2_PSK; config.ap.max_connection=1; config.ap.channel=1;
    for(unsigned i=0;i<4;++i)snprintf(s_token+i*8,9,"%08" PRIx32,esp_random());
    s_record_count=0;
    wifi_scan_config_t scan={0};
    if (esp_wifi_scan_start(&scan, true) == ESP_OK) {
        s_record_count = 16;
        if (esp_wifi_scan_get_ap_records(&s_record_count, s_records) != ESP_OK) {
            s_record_count = 0;
            esp_wifi_clear_ap_list();
        }
    }
    err=esp_wifi_set_mode(WIFI_MODE_APSTA);
    if(err==ESP_OK)err=esp_wifi_set_config(WIFI_IF_AP,&config);
    if(err!=ESP_OK)goto done;
    esp_netif_ip_info_t ip; err=esp_netif_get_ip_info(ap,&ip); if(err!=ESP_OK)goto done;
    s_ap_address=ip.ip.addr;
    httpd_config_t http=HTTPD_DEFAULT_CONFIG(); http.max_open_sockets=3; http.lru_purge_enable=true;
    http.stack_size=6144; http.recv_wait_timeout=5; http.send_wait_timeout=5;
    err=httpd_start(&server,&http); if(err!=ESP_OK)goto done;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=root_get},
        {.uri="/networks",.method=HTTP_GET,.handler=networks_get},
        {.uri="/status",.method=HTTP_GET,.handler=status_get},
        {.uri="/configure",.method=HTTP_POST,.handler=configure_post}};
    for(unsigned i=0;i<4;++i){err=httpd_register_uri_handler(server,&routes[i]);if(err!=ESP_OK)goto done;}
    atomic_store(&s_status,0);
    display((char *)config.ap.ssid,(char *)config.ap.password);
    for(;;){
        credentials_t value;
        xQueueReceive(s_pending,&value,portMAX_DELAY);
        /* Drain the previous disconnect before clearing readiness, so a stale
         * GOT_IP cannot validate a different credential submission. */
        xEventGroupClearBits(bits, ready << 1);
        wifi_ap_record_t previous;
        bool was_connected = esp_wifi_sta_get_ap_info(&previous) == ESP_OK;
        esp_err_t disconnected = esp_wifi_disconnect();
        if (was_connected && disconnected == ESP_OK &&
            !(xEventGroupWaitBits(bits, ready << 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(2000)) & (ready << 1))) {
            atomic_store(&s_status, 2);
            memset(&value, 0, sizeof(value));
            continue;
        }
        xEventGroupClearBits(bits,ready);
        wifi_config_t sta={0}; memcpy(sta.sta.ssid,value.ssid,strlen(value.ssid)); memcpy(sta.sta.password,value.password,strlen(value.password));
        sta.sta.threshold.authmode = value.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        err=esp_wifi_set_config(WIFI_IF_STA,&sta);
        if(err==ESP_OK)err=esp_wifi_connect();
        bool connected=err==ESP_OK && (xEventGroupWaitBits(bits,ready,pdFALSE,pdTRUE,pdMS_TO_TICKS(25000)) & ready);
        wifi_ap_record_t joined;
        connected = connected && esp_wifi_sta_get_ap_info(&joined) == ESP_OK &&
            strncmp((char *)joined.ssid, value.ssid, 32) == 0;
        if(!connected){esp_wifi_disconnect();atomic_store(&s_status,2);memset(&value,0,sizeof(value));continue;}
        nvs_handle_t nvs; err=nvs_open("fmo_wifi",NVS_READWRITE,&nvs);
        if(err==ESP_OK){err=nvs_set_blob(nvs,"credentials",&value,sizeof(value));if(err==ESP_OK)err=nvs_set_u8(nvs,"setup",0);if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
        memset(&value,0,sizeof(value)); memset(&sta,0,sizeof(sta));
        if(err!=ESP_OK){atomic_store(&s_status,4);continue;}
        atomic_store(&s_status,3); vTaskDelay(pdMS_TO_TICKS(4000)); break;
    }
done:
    if(server)httpd_stop(server);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_netif_destroy_default_wifi(ap);
    vQueueDelete(s_pending); s_pending=NULL;
    memset(&config,0,sizeof(config));memset(s_token,0,sizeof(s_token));
    return err;
}
