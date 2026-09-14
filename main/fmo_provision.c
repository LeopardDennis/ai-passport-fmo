#include "fmo_provision.h"
#include "fmo_credentials.h"
#include "fmo_wifi_profiles.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include <stdatomic.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef fmo_wifi_credential_t credentials_t;
typedef struct { credentials_t value; bool remove; bool finish; } setup_request_t;
static fmo_wifi_profiles_t s_profiles;
static SemaphoreHandle_t s_profiles_lock;
static esp_err_t save_profiles(const fmo_wifi_profiles_t *profiles, bool finish)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("fmo_wifi", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, "profiles_v1", profiles, sizeof(*profiles));
    if (err == ESP_OK && finish) err = nvs_set_u8(nvs, "setup", 0);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err == ESP_OK) s_profiles = *profiles;
    return err;
}
static void to_wifi_config(wifi_config_t *config, const credentials_t *value)
{
    memset(config, 0, sizeof(*config));
    memcpy(config->sta.ssid, value->ssid, strlen(value->ssid));
    memcpy(config->sta.password, value->password, strlen(value->password));
    config->sta.threshold.authmode = value->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
}
static QueueHandle_t s_pending;
static atomic_int s_status; /* 0 ready, 1 testing, 2 failed, 3 saved, 4 storage error, 5 deleted, 6 full */
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
"<p id='result' role='status'></p><h2>已保存网络（最多 5 组）</h2><div id='saved'></div>"
"<button id='finish' type='button'>完成并连接已保存网络</button><script>const token='";
static const char page_end[] =
"';const form=document.getElementById('form'),ssid=document.getElementById('ssid'),pass=document.getElementById('pass'),net=document.getElementById('net'),save=document.getElementById('save'),result=document.getElementById('result');"
"fetch('/networks').then(r=>r.json()).then(a=>a.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=n;net.append(o)})).catch(()=>{});"
"net.onchange=()=>{if(net.value)ssid.value=net.value};"
"const saved=document.getElementById('saved'),finish=document.getElementById('finish');"
"async function refreshSaved(){const r=await fetch('/saved',{cache:'no-store'});if(!r.ok)throw Error();const a=await r.json();saved.replaceChildren();a.forEach(n=>{const row=document.createElement('div'),text=document.createElement('span'),button=document.createElement('button');text.textContent=n;button.textContent='删除';button.type='button';button.onclick=()=>{if(confirm('删除已保存网络 '+n+'？'))submit({ssid:n,password:'',remove:true})};row.append(text,button);saved.append(row)});finish.disabled=a.length===0}"
"refreshSaved().catch(()=>{result.textContent='读取列表失败，请刷新。'});finish.onclick=()=>submit({ssid:'',password:'',finish:true});"
"async function poll(){try{const r=await fetch('/status',{cache:'no-store'});const s=await r.json();"
"if(s===3){result.textContent='配网成功，热点即将关闭。设备正在连接 FMO。';pass.value='';return}"
"if(s===5){result.textContent='已删除';save.disabled=false;await refreshSaved();return}"
"if(s===6){result.textContent='已满 5 组，请先删除一个网络。';save.disabled=false;return}"
"if(s===2||s===4){result.textContent=s===2?'连接失败，请检查密码和信号后重试。':'保存失败，旧配置未被主动删除。请重试。';save.disabled=false;return}"
"result.textContent='正在验证连接，请稍候…';setTimeout(poll,1000)}catch(e){result.textContent='请查看设备屏幕；若仍在配网，请重新连接热点并刷新。';save.disabled=false}}"
"async function submit(data){save.disabled=true;try{const r=await fetch('/configure',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Token':token},body:JSON.stringify(data)});"
"if(!r.ok)throw Error();poll()}catch(e){result.textContent='提交失败，请检查输入或等待当前操作完成。';save.disabled=false}}"
"form.onsubmit=e=>{e.preventDefault();submit({ssid:ssid.value,password:pass.value})};</script></html>";

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
static esp_err_t saved_get(httpd_req_t *req)
{
    if (!local_request(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "AP only");
    cJSON *array = cJSON_CreateArray();
    if (!array) return ESP_ERR_NO_MEM;
    xSemaphoreTake(s_profiles_lock, portMAX_DELAY);
    for (unsigned i = 0; i < s_profiles.count; ++i)
        cJSON_AddItemToArray(array, cJSON_CreateString(s_profiles.entries[i].ssid));
    xSemaphoreGive(s_profiles_lock);
    char *json = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    if (!json) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, json);
    cJSON_free(json);
    return err;
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
    setup_request_t request = {0};
    request.remove = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "remove"));
    request.finish = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "finish"));
    bool valid = !(request.remove && request.finish) && cJSON_IsString(ssid) && cJSON_IsString(password) &&
        (request.finish ? !ssid->valuestring[0] && !password->valuestring[0] :
         fmo_credentials_valid(ssid->valuestring, strlen(ssid->valuestring), password->valuestring, strlen(password->valuestring)));
    if (valid) {strcpy(request.value.ssid, ssid->valuestring); strcpy(request.value.password, password->valuestring);}
    cJSON_Delete(json); memset(body, 0, sizeof(body));
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid credentials");
    int status = atomic_load(&s_status);
    if (status == 1 || status == 3) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Busy");
    atomic_store(&s_status, 1);
    if (xQueueSend(s_pending, &request, 0) != pdTRUE) {atomic_store(&s_status, status); return ESP_FAIL;}
    memset(&request, 0, sizeof(request));
    return httpd_resp_sendstr(req, "Accepted");
}

bool fmo_provision_load(wifi_config_t *config, bool *force)
{
    *force = false;
    if (!s_profiles_lock) s_profiles_lock = xSemaphoreCreateMutex();
    if (!s_profiles_lock) return false;
    fmo_wifi_profiles_init(&s_profiles);
    nvs_handle_t nvs;
    if (nvs_open("fmo_wifi", NVS_READONLY, &nvs) != ESP_OK) return false;
    uint8_t flag = 0;
    nvs_get_u8(nvs, "setup", &flag);
    *force = flag != 0;
    fmo_wifi_profiles_t profiles;
    size_t size = sizeof(profiles);
    esp_err_t err = nvs_get_blob(nvs, "profiles_v1", &profiles, &size);
    bool migrate = false;
    if (err == ESP_OK && size == sizeof(profiles) && fmo_wifi_profiles_valid(&profiles)) {
        s_profiles = profiles;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        credentials_t old = {0};
        size = sizeof(old);
        err = nvs_get_blob(nvs, "credentials", &old, &size);
        if (err == ESP_OK && size == sizeof(old))
            migrate = fmo_wifi_profiles_put(&s_profiles, &old);
        memset(&old, 0, sizeof(old));
    }
    nvs_close(nvs);
    /* Keep the legacy blob intact on migration failure. An empty new-format
     * list is authoritative, so deleting the last entry never resurrects it. */
    if (migrate) save_profiles(&s_profiles, false);
    if (!s_profiles.count) return false;
    to_wifi_config(config, &s_profiles.entries[s_profiles.preferred]);
    return true;
}

uint8_t fmo_provision_preferred_mask(void)
{
    return s_profiles.count ? (uint8_t)(1u << s_profiles.preferred) : 0;
}

void fmo_provision_remember_connected(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;
    ap.ssid[32] = 0;
    int index = fmo_wifi_profiles_find(&s_profiles, (char *)ap.ssid);
    if (index < 0 || index == s_profiles.preferred) return;
    fmo_wifi_profiles_t updated = s_profiles;
    updated.preferred = (uint8_t)index;
    save_profiles(&updated, false);
}

bool fmo_provision_next(wifi_config_t *config, uint8_t *tried)
{
    int8_t rssi[FMO_WIFI_PROFILE_MAX];
    memset(rssi, -128, sizeof(rssi));
    if (!s_profiles.count) return false;
    if (*tried != 0) {
        wifi_scan_config_t scan = {0};
        if (esp_wifi_scan_start(&scan, true) == ESP_OK) {
            uint16_t count = 16;
            if (esp_wifi_scan_get_ap_records(&count, s_records) == ESP_OK) {
                for (unsigned i = 0; i < count; ++i) {
                    s_records[i].ssid[32] = 0;
                    int j = fmo_wifi_profiles_find(&s_profiles, (char *)s_records[i].ssid);
                    if (j >= 0 && s_records[i].rssi > rssi[j]) rssi[j] = s_records[i].rssi;
                }
            } else esp_wifi_clear_ap_list();
        }
    }
    int index = fmo_wifi_profiles_pick(&s_profiles, *tried, rssi, *tried == 0);
    if (index < 0) return false;
    *tried |= (uint8_t)(1u << index);
    to_wifi_config(config, &s_profiles.entries[index]);
    return true;
}
esp_err_t fmo_provision_force(void)
{
    nvs_handle_t nvs; esp_err_t err=nvs_open("fmo_wifi",NVS_READWRITE,&nvs);
    if (err != ESP_OK) return err;
    err=nvs_set_u8(nvs,"setup",1); if(err == ESP_OK)err=nvs_commit(nvs); nvs_close(nvs); return err;
}
esp_err_t fmo_provision_run(EventGroupHandle_t bits, EventBits_t ready, fmo_setup_display_t display)
{
    if (!s_profiles_lock) return ESP_ERR_NO_MEM;
    s_pending=xQueueCreate(1,sizeof(setup_request_t)); if(!s_pending)return ESP_ERR_NO_MEM;
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
        {.uri="/saved",.method=HTTP_GET,.handler=saved_get},
        {.uri="/configure",.method=HTTP_POST,.handler=configure_post}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i){err=httpd_register_uri_handler(server,&routes[i]);if(err!=ESP_OK)goto done;}
    atomic_store(&s_status,0);
    display((char *)config.ap.ssid,(char *)config.ap.password);
    for(;;){
        setup_request_t request;
        xQueueReceive(s_pending,&request,portMAX_DELAY);
        credentials_t value = request.value;
        fmo_wifi_profiles_t updated;
        xSemaphoreTake(s_profiles_lock, portMAX_DELAY);
        updated = s_profiles;
        xSemaphoreGive(s_profiles_lock);
        if (request.remove) {
            if (!fmo_wifi_profiles_remove(&updated, value.ssid)) {
                atomic_store(&s_status, 2);
                continue;
            }
            xSemaphoreTake(s_profiles_lock, portMAX_DELAY);
            err = save_profiles(&updated, false);
            xSemaphoreGive(s_profiles_lock);
            atomic_store(&s_status, err == ESP_OK ? 5 : 4);
            memset(&updated, 0, sizeof(updated));
            continue;
        }
        if (request.finish) {
            if (!updated.count) {atomic_store(&s_status, 2); continue;}
            xSemaphoreTake(s_profiles_lock, portMAX_DELAY);
            err = save_profiles(&updated, true);
            xSemaphoreGive(s_profiles_lock);
            if (err != ESP_OK) {atomic_store(&s_status, 4); continue;}
            wifi_config_t selected;
            to_wifi_config(&selected, &updated.entries[updated.preferred]);
            esp_wifi_disconnect();
            xEventGroupClearBits(bits, ready);
            err = esp_wifi_set_config(WIFI_IF_STA, &selected);
            memset(&selected, 0, sizeof(selected));
            memset(&updated, 0, sizeof(updated));
            if (err != ESP_OK) {atomic_store(&s_status, 2); continue;}
            atomic_store(&s_status, 3);
            vTaskDelay(pdMS_TO_TICKS(4000));
            break;
        }
        if (!fmo_wifi_profiles_put(&updated, &value)) {
            atomic_store(&s_status, 6);
            memset(&updated, 0, sizeof(updated));
            memset(&value, 0, sizeof(value));
            continue;
        }
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
        xSemaphoreTake(s_profiles_lock, portMAX_DELAY);
        err = save_profiles(&updated, true);
        xSemaphoreGive(s_profiles_lock);
        memset(&updated, 0, sizeof(updated));
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
