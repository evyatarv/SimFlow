#include <string.h>
#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

#include "sf_locator.h"
#include "sf_locator_web.h"
#include "sf_locator_led.h"
#include "sf_locator_mpu.h"

#define NVS_NAMESPACE       "simplebot"
#define NVS_KEY_AP_SSID     "ap_ssid"
#define NVS_KEY_AP_PASS     "ap_pass"

#define HTTPD_STACK_SIZE    8192
#define AP_RESTART_DELAY_MS 200

static const char *TAG = "SF_LOCATOR_WEB";

static httpd_handle_t   s_httpd = NULL;
static char             s_ap_ssid[SF_LOCATOR_AP_SSID_MAX_LEN + 1] = SF_LOCATOR_AP_SSID_DEFAULT;
static char             s_ap_pass[SF_LOCATOR_AP_PASS_MAX_LEN + 1] = SF_LOCATOR_AP_PASS_DEFAULT;
static bool             s_ap_restart_pending = false;
static uint32_t         s_ap_restart_at_ms = 0;

/* index.html is built from locator_app/webui/index.html and embedded by the
 * CMake EMBED_TXTFILES directive. The linker provides _start/_end symbols. */
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

static void load_ap_creds_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t len = sizeof(s_ap_ssid);
    if (nvs_get_str(h, NVS_KEY_AP_SSID, s_ap_ssid, &len) != ESP_OK) {
        strncpy(s_ap_ssid, SF_LOCATOR_AP_SSID_DEFAULT, sizeof(s_ap_ssid) - 1);
    }
    len = sizeof(s_ap_pass);
    if (nvs_get_str(h, NVS_KEY_AP_PASS, s_ap_pass, &len) != ESP_OK) {
        strncpy(s_ap_pass, SF_LOCATOR_AP_PASS_DEFAULT, sizeof(s_ap_pass) - 1);
    }
    nvs_close(h);

    if (s_ap_pass[0] != '\0' && strlen(s_ap_pass) < 8) {
        strncpy(s_ap_pass, SF_LOCATOR_AP_PASS_DEFAULT, sizeof(s_ap_pass) - 1);
    }
}

static sf_err_t save_ap_creds_to_nvs(void)
{
    sf_err_t status = SF_FAIL;
    nvs_handle_t h;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "nvs_open with status: %d", err);

    err = nvs_set_str(h, NVS_KEY_AP_SSID, s_ap_ssid);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, close, "nvs_set_str ssid with status: %d", err);

    err = nvs_set_str(h, NVS_KEY_AP_PASS, s_ap_pass);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, close, "nvs_set_str pass with status: %d", err);

    err = nvs_commit(h);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, close, "nvs_commit with status: %d", err);

    status = SF_OK;
close:
    nvs_close(h);
end:
    return status;
}

sf_err_t sf_locator_web_start_softap(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;
    wifi_mode_t mode = WIFI_MODE_NULL;

    err = esp_wifi_get_mode(&mode);
    if (err == ESP_OK && mode == WIFI_MODE_STA) {
        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                          "esp_wifi_set_mode APSTA with status: %d", err);
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_get_mode returned %d - continuing", err);
    }

    wifi_config_t ap_cfg = { 0 };
    size_t ssid_len = strlen(s_ap_ssid);
    if (ssid_len > sizeof(ap_cfg.ap.ssid)) {
        ssid_len = sizeof(ap_cfg.ap.ssid);
    }
    memcpy(ap_cfg.ap.ssid, s_ap_ssid, ssid_len);
    ap_cfg.ap.ssid_len  = ssid_len;
    ap_cfg.ap.channel   = 1;
    ap_cfg.ap.max_connection = 4;
    if (s_ap_pass[0] == '\0') {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)ap_cfg.ap.password, s_ap_pass, sizeof(ap_cfg.ap.password) - 1);
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "esp_wifi_set_config AP with status: %d", err);

    ESP_LOGI(TAG, "Soft-AP running SSID='%s'", s_ap_ssid);
    status = SF_OK;
end:
    return status;
}

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start,
                           index_html_end - index_html_start);
}

static esp_err_t handler_status(httpd_req_t *req)
{
    char ip_str[16] = "0.0.0.0";
    esp_netif_t *ap_if = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_if) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(ap_if, &ip) == ESP_OK) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip.ip));
        }
    }

    sf_mpu_status_t mpu;
    sf_locator_mpu_get_status(&mpu);

    uint8_t r, g, b;
    sf_locator_led_get_raw(&r, &g, &b);

    cJSON *root = cJSON_CreateObject();
    cJSON *ap = cJSON_AddObjectToObject(root, "ap");
    cJSON_AddStringToObject(ap, "ssid", s_ap_ssid);
    cJSON_AddStringToObject(ap, "ip", ip_str);

    cJSON *mpu_obj = cJSON_AddObjectToObject(root, "mpu");
    cJSON_AddBoolToObject(mpu_obj, "ready", mpu.ready);
    cJSON_AddNumberToObject(mpu_obj, "baselineAz", mpu.baseline_az);
    if (isnan(mpu.last_temperature_c)) {
        cJSON_AddNullToObject(mpu_obj, "temperatureC");
    } else {
        cJSON_AddNumberToObject(mpu_obj, "temperatureC", mpu.last_temperature_c);
    }

    cJSON *led = cJSON_AddObjectToObject(root, "led");
    cJSON *raw = cJSON_AddObjectToObject(led, "raw");
    cJSON_AddNumberToObject(raw, "r", r);
    cJSON_AddNumberToObject(raw, "g", g);
    cJSON_AddNumberToObject(raw, "b", b);
    cJSON_AddBoolToObject(led, "rawActive", sf_locator_led_get_raw_active());
    cJSON_AddStringToObject(led, "color",
                            sf_locator_led_color_name(sf_locator_led_get_color()));

    char *body = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);

    cJSON_free(body);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t read_json_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    int total = req->content_len;
    if (total <= 0 || (size_t)total >= buf_len) {
        return ESP_FAIL;
    }
    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            return ESP_FAIL;
        }
        received += r;
    }
    buf[received] = '\0';
    return ESP_OK;
}

static esp_err_t handler_ap(httpd_req_t *req)
{
    char buf[256];
    if (read_json_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"message\":\"Missing JSON body\"}");
    }

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"message\":\"Invalid JSON\"}");
    }

    const cJSON *ssid_j = cJSON_GetObjectItem(root, "ssid");
    const cJSON *pass_j = cJSON_GetObjectItem(root, "password");
    const char *ssid = (cJSON_IsString(ssid_j) ? ssid_j->valuestring : "");
    const char *pass = (cJSON_IsString(pass_j) ? pass_j->valuestring : "");

    esp_err_t ret;
    if (ssid[0] == '\0') {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        ret = httpd_resp_sendstr(req, "{\"message\":\"SSID cannot be empty\"}");
        cJSON_Delete(root);
        return ret;
    }
    size_t plen = strlen(pass);
    if (plen > 0 && plen < 8) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        ret = httpd_resp_sendstr(req, "{\"message\":\"Password must be >= 8 chars or empty\"}");
        cJSON_Delete(root);
        return ret;
    }

    strncpy(s_ap_ssid, ssid, sizeof(s_ap_ssid) - 1);
    s_ap_ssid[sizeof(s_ap_ssid) - 1] = '\0';
    strncpy(s_ap_pass, pass, sizeof(s_ap_pass) - 1);
    s_ap_pass[sizeof(s_ap_pass) - 1] = '\0';

    save_ap_creds_to_nvs();

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    s_ap_restart_pending = true;
    s_ap_restart_at_ms = now_ms + AP_RESTART_DELAY_MS;

    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_sendstr(req,
        "{\"message\":\"AP credentials saved. Reconnecting Soft-AP...\"}");
    cJSON_Delete(root);
    return ret;
}

static esp_err_t handler_calibrate(httpd_req_t *req)
{
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    sf_err_t ok = sf_locator_mpu_calibrate(now_ms);
    httpd_resp_set_type(req, "application/json");
    if (ok == SF_OK) {
        return httpd_resp_sendstr(req, "{\"message\":\"MPU baseline calibrated\"}");
    }
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "{\"message\":\"Calibration failed\"}");
}

static esp_err_t handler_led(httpd_req_t *req)
{
    char buf[256];
    if (read_json_body(req, buf, sizeof(buf)) != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"message\":\"Missing JSON body\"}");
    }

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"message\":\"Invalid JSON\"}");
    }

    esp_err_t ret;
    httpd_resp_set_type(req, "application/json");

    cJSON *r_j = cJSON_GetObjectItem(root, "r");
    cJSON *g_j = cJSON_GetObjectItem(root, "g");
    cJSON *b_j = cJSON_GetObjectItem(root, "b");
    if (r_j || g_j || b_j) {
        if (!(cJSON_IsNumber(r_j) && cJSON_IsNumber(g_j) && cJSON_IsNumber(b_j))) {
            httpd_resp_set_status(req, "400 Bad Request");
            ret = httpd_resp_sendstr(req,
                "{\"message\":\"r, g, b must be integers (0-255)\"}");
            cJSON_Delete(root);
            return ret;
        }
        int r = r_j->valueint;
        int g = g_j->valueint;
        int b = b_j->valueint;
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
            httpd_resp_set_status(req, "400 Bad Request");
            ret = httpd_resp_sendstr(req,
                "{\"message\":\"r, g, b must be within 0-255\"}");
            cJSON_Delete(root);
            return ret;
        }
        sf_locator_led_set_raw((uint8_t)r, (uint8_t)g, (uint8_t)b);

        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "message", "LED updated (raw RGB)");
        cJSON_AddNumberToObject(res, "r", r);
        cJSON_AddNumberToObject(res, "g", g);
        cJSON_AddNumberToObject(res, "b", b);
        char *body = cJSON_PrintUnformatted(res);
        ret = httpd_resp_sendstr(req, body);
        cJSON_free(body);
        cJSON_Delete(res);
        cJSON_Delete(root);
        return ret;
    }

    const cJSON *color_j = cJSON_GetObjectItem(root, "color");
    const char *color = cJSON_IsString(color_j) ? color_j->valuestring : "off";

    sf_led_color_t c;
    if (strcmp(color, "red") == 0) {
        c = SF_LED_COLOR_RED;
    } else if (strcmp(color, "green") == 0) {
        c = SF_LED_COLOR_GREEN;
    } else if (strcmp(color, "blue") == 0) {
        c = SF_LED_COLOR_BLUE;
    } else if (strcmp(color, "off") == 0) {
        c = SF_LED_COLOR_OFF;
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        ret = httpd_resp_sendstr(req,
            "{\"message\":\"Color must be red, green, blue, or off, or send r/g/b\"}");
        cJSON_Delete(root);
        return ret;
    }

    sf_locator_led_set_named(c);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "message", "LED updated");
    cJSON_AddStringToObject(res, "color", color);
    char *body = cJSON_PrintUnformatted(res);
    ret = httpd_resp_sendstr(req, body);
    cJSON_free(body);
    cJSON_Delete(res);
    cJSON_Delete(root);
    return ret;
}

static sf_err_t start_http_server(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size  = HTTPD_STACK_SIZE;
    cfg.lru_purge_enable = true;

    err = httpd_start(&s_httpd, &cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "httpd_start with status: %d", err);

    httpd_uri_t u_root = { .uri="/", .method=HTTP_GET, .handler=handler_root };
    httpd_register_uri_handler(s_httpd, &u_root);

    httpd_uri_t u_status = { .uri="/api/status", .method=HTTP_GET, .handler=handler_status };
    httpd_register_uri_handler(s_httpd, &u_status);

    httpd_uri_t u_ap = { .uri="/api/ap", .method=HTTP_POST, .handler=handler_ap };
    httpd_register_uri_handler(s_httpd, &u_ap);

    httpd_uri_t u_cal = { .uri="/api/calibrate", .method=HTTP_POST, .handler=handler_calibrate };
    httpd_register_uri_handler(s_httpd, &u_cal);

    httpd_uri_t u_led = { .uri="/api/led", .method=HTTP_POST, .handler=handler_led };
    httpd_register_uri_handler(s_httpd, &u_led);

    ESP_LOGI(TAG, "HTTP server started on port 80");
    status = SF_OK;
end:
    return status;
}

sf_err_t sf_locator_web_init(void)
{
    sf_err_t status = SF_FAIL;

    load_ap_creds_from_nvs();

    if (sf_locator_web_start_softap() != SF_OK) {
        goto end;
    }
    if (start_http_server() != SF_OK) {
        goto end;
    }

    status = SF_OK;
end:
    return status;
}

void sf_locator_web_tick(uint32_t now_ms)
{
    if (s_ap_restart_pending && now_ms >= s_ap_restart_at_ms) {
        s_ap_restart_pending = false;
        sf_locator_web_start_softap();
    }
}

void sf_locator_web_get_ap_ssid(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    strncpy(out, s_ap_ssid, out_len - 1);
    out[out_len - 1] = '\0';
}
