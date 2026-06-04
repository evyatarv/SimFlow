#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "sf_time.h"
#include "sf_wifi.h"
#include "sf_wifi_prov.h"

/* ------------------------------------------------------------------ */
/* Defines                                                              */
/* ------------------------------------------------------------------ */

#define NVS_NAMESPACE       "sf_prov"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "password"

#define MAX_SSID_LEN        33
#define MAX_PASS_LEN        65
#define MAX_BODY_LEN        512

#define TIMEOUT_TIMED_US    (5ULL * 60 * 1000000)   /* 5 min */
#define REBOOT_DELAY_US     (3ULL * 1000000)         /* 3 s   */

static const char *TAG = "SF_WIFI_PROV";

/* ------------------------------------------------------------------ */
/* Internal event base                                                  */
/* ------------------------------------------------------------------ */

ESP_EVENT_DEFINE_BASE(SF_PROV_EVENT);

typedef enum {
    EVT_TIME_VALID = 0,
    EVT_POST_TIME,
    EVT_POST_CONNECT,
    EVT_TIMEOUT,
    EVT_BUTTON,
} prov_evt_id_t;

/* ------------------------------------------------------------------ */
/* Internal AP state                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    AP_CLOSED = 0,
    AP_OPEN_INDEFINITE,
    AP_OPEN_TIMED,
} ap_state_t;

/* ------------------------------------------------------------------ */
/* Static state                                                         */
/* ------------------------------------------------------------------ */

static sf_wifi_prov_config_t s_config;
static ap_state_t            s_ap_state   = AP_CLOSED;
static httpd_handle_t        s_httpd      = NULL;
static esp_timer_handle_t    s_reboot_timer  = NULL;
static esp_timer_handle_t    s_timeout_timer = NULL;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static void ap_open_indefinite(void);
static void ap_open_timed(void);
static void ap_close(void);
static esp_err_t start_httpd(void);
static void stop_httpd(void);

/* ------------------------------------------------------------------ */
/* Helpers — URL decode / field extraction / time parsing               */
/* ------------------------------------------------------------------ */

static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dst_size; si++) {
        if (src[si] == '%' && src[si + 1] && src[si + 2]) {
            char hex[3] = {src[si + 1], src[si + 2], '\0'};
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else if (src[si] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static bool get_field(const char *body, const char *key,
                       char *value, size_t value_size)
{
    size_t key_len = strlen(key);
    const char *p = body;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            const char *end = p;
            while (*end && *end != '&') end++;
            char encoded[MAX_BODY_LEN] = {0};
            size_t len = (size_t)(end - p);
            if (len >= sizeof(encoded)) len = sizeof(encoded) - 1;
            memcpy(encoded, p, len);
            url_decode(value, encoded, value_size);
            return true;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    return false;
}

static time_t parse_datetime(const char *str)
{
    struct tm tm = {0};
    if (strptime(str, "%Y-%m-%dT%H:%M", &tm) == NULL) return -1;
    tm.tm_isdst = -1;  /* let mktime determine DST using device TZ */
    return mktime(&tm);
}

/* ------------------------------------------------------------------ */
/* Helpers — body reading                                               */
/* ------------------------------------------------------------------ */

static int read_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    int content_len = req->content_len;
    if (content_len <= 0 || content_len >= (int)buf_size) return -1;

    int received = 0;
    while (received < content_len) {
        int ret = httpd_req_recv(req, buf + received, content_len - received);
        if (ret <= 0) return -1;
        received += ret;
    }
    buf[received] = '\0';
    return received;
}

/* ------------------------------------------------------------------ */
/* Helpers — file streaming                                             */
/* ------------------------------------------------------------------ */

static esp_err_t stream_file(httpd_req_t *req, const char *path,
                              const char *content_type)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s", path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, content_type);
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, (ssize_t)n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* NVS                                                                  */
/* ------------------------------------------------------------------ */

static bool nvs_read_creds(char *ssid, size_t ssid_size,
                            char *password, size_t pass_size)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = (nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_size) == ESP_OK &&
               nvs_get_str(h, NVS_KEY_PASS, password, &pass_size) == ESP_OK);
    nvs_close(h);
    return ok;
}

static sf_err_t nvs_write_creds(const char *ssid, const char *password)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGE, TAG, err, "nvs_open: %d", err);
    esp_err_t s1 = nvs_set_str(h, NVS_KEY_SSID, ssid);
    esp_err_t s2 = nvs_set_str(h, NVS_KEY_PASS, password);
    esp_err_t s3 = nvs_commit(h);
    nvs_close(h);
    if (s1 != ESP_OK || s2 != ESP_OK || s3 != ESP_OK) {
        ESP_LOGE(TAG, "NVS write failed: %d %d %d", s1, s2, s3);
        return SF_FAIL;
    }
    return SF_OK;
}

/* ------------------------------------------------------------------ */
/* HTTP handlers                                                        */
/* ------------------------------------------------------------------ */

static esp_err_t get_root_handler(httpd_req_t *req)
{
    return stream_file(req, s_config.prov_html_path, "text/html");
}

static esp_err_t get_logo_handler(httpd_req_t *req)
{
    if (!s_config.logo_path) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    return stream_file(req, s_config.logo_path, "image/png");
}

static esp_err_t get_api_time_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    if (!sf_time_is_valid()) {
        httpd_resp_sendstr(req, "{\"time\":null}");
        return ESP_OK;
    }
    time_t now = 0;
    time(&now);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    char json[48];
    snprintf(json, sizeof(json), "{\"time\":\"%s\"}", ts);
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static esp_err_t post_time_handler(httpd_req_t *req)
{
    char body[MAX_BODY_LEN];
    if (read_body(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_OK;
    }
    char time_str[32] = {0};
    if (get_field(body, "time", time_str, sizeof(time_str))) {
        time_t epoch = parse_datetime(time_str);
        if (epoch > 0) sf_time_set_manual(epoch);
    }
    /* §6.5: Send response BEFORE posting event — httpd_stop() may be called
     * downstream and will wait for active handlers to finish. */
    httpd_resp_sendstr(req, "OK");
    esp_event_post(SF_PROV_EVENT, EVT_POST_TIME, NULL, 0, pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t post_connect_handler(httpd_req_t *req)
{
    char body[MAX_BODY_LEN];
    if (read_body(req, body, sizeof(body)) < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_OK;
    }
    char ssid[MAX_SSID_LEN]     = {0};
    char password[MAX_PASS_LEN] = {0};
    if (!get_field(body, "ssid", ssid, sizeof(ssid)) ||
        !get_field(body, "password", password, sizeof(password))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
        return ESP_OK;
    }
    /* Optional time field */
    char time_str[32] = {0};
    if (get_field(body, "time", time_str, sizeof(time_str))) {
        time_t epoch = parse_datetime(time_str);
        if (epoch > 0) sf_time_set_manual(epoch);
    }
    if (nvs_write_creds(ssid, password) != SF_OK) {
        ESP_LOGE(TAG, "NVS write failed — aborting reboot");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
        return ESP_OK;
    }

    /* §6.5 + §9.3: Stream success.html FIRST, then post event.
     * The 3s reboot timer (started by EVT_POST_CONNECT) gives TCP time
     * to fully transmit the page before the device disappears. */
    stream_file(req, s_config.success_html_path, "text/html");
    esp_event_post(SF_PROV_EVENT, EVT_POST_CONNECT, NULL, 0, pdMS_TO_TICKS(100));
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* HTTP server lifecycle                                                */
/* ------------------------------------------------------------------ */

static esp_err_t start_httpd(void)
{
    if (s_httpd) return ESP_OK;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 5;
    esp_err_t err = httpd_start(&s_httpd, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %d", err);
        return err;
    }
    static const httpd_uri_t uris[] = {
        { .uri = "/",         .method = HTTP_GET,  .handler = get_root_handler      },
        { .uri = "/logo.png", .method = HTTP_GET,  .handler = get_logo_handler      },
        { .uri = "/api/time", .method = HTTP_GET,  .handler = get_api_time_handler  },
        { .uri = "/time",     .method = HTTP_POST, .handler = post_time_handler     },
        { .uri = "/connect",  .method = HTTP_POST, .handler = post_connect_handler  },
    };
    for (int i = 0; i < (int)(sizeof(uris) / sizeof(uris[0])); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

static void stop_httpd(void)
{
    if (!s_httpd) return;
    httpd_stop(s_httpd);
    s_httpd = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
}

/* ------------------------------------------------------------------ */
/* Timer callbacks                                                      */
/* ------------------------------------------------------------------ */

static void reboot_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

static void timeout_timer_cb(void *arg)
{
    esp_event_post(SF_PROV_EVENT, EVT_TIMEOUT, NULL, 0, pdMS_TO_TICKS(100));
}

/* ------------------------------------------------------------------ */
/* AP state transitions                                                 */
/* ------------------------------------------------------------------ */

static void notify_state_change(sf_wifi_prov_state_t state)
{
    if (s_config.on_state_change) {
        s_config.on_state_change(state, s_config.state_ctx);
    }
}

static void ap_open_indefinite(void)
{
    SF_CHECK_EXPR_RETURN(ESP_LOGW, TAG, s_ap_state != AP_CLOSED, "already open, skip");
    
    SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, sf_wifi_ap_start(s_config.ap_ssid) != SF_OK,
                         "ap_open_indefinite: sf_wifi_ap_start failed");
    
    esp_err_t http_err = start_httpd();
    
    if (http_err != ESP_OK) 
        sf_wifi_ap_stop();
    
    SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, http_err != ESP_OK,
                         "ap_open_indefinite: start_httpd failed");

    s_ap_state = AP_OPEN_INDEFINITE;
    
    notify_state_change(SF_WIFI_PROV_PROVISIONING);
    
    ESP_LOGI(TAG, "SoftAP open (indefinite)");
}

static void ap_open_timed(void)
{
    SF_CHECK_EXPR_RETURN(ESP_LOGW, TAG, s_ap_state == AP_OPEN_TIMED,
                         "already timed, no extension");

    bool was_closed = (s_ap_state == AP_CLOSED);
    if (was_closed) {
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, sf_wifi_ap_start(s_config.ap_ssid) != SF_OK,
                             "ap_open_timed: sf_wifi_ap_start failed");
        esp_err_t http_err = start_httpd();
        if (http_err != ESP_OK) sf_wifi_ap_stop();
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, http_err != ESP_OK,
                             "ap_open_timed: start_httpd failed");
    }
    /* Start the 5-min absolute countdown */
    esp_timer_start_once(s_timeout_timer, TIMEOUT_TIMED_US);
    s_ap_state = AP_OPEN_TIMED;
    if (was_closed) {
        notify_state_change(SF_WIFI_PROV_PROVISIONING);
    }
    /* OPEN_INDEFINITE → OPEN_TIMED stays PROVISIONING — no callback needed */
    ESP_LOGI(TAG, "SoftAP open (5-min window)");
}

static void ap_close(void)
{
    if (s_ap_state == AP_CLOSED) return;
    esp_timer_stop(s_timeout_timer);
    stop_httpd();
    sf_wifi_ap_stop();
    s_ap_state = AP_CLOSED;
    notify_state_change(SF_WIFI_PROV_CLOSED);
    ESP_LOGI(TAG, "SoftAP closed");
}

/* ------------------------------------------------------------------ */
/* Event handlers                                                       */
/* ------------------------------------------------------------------ */

static void prov_state_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    switch ((prov_evt_id_t)id) {
        case EVT_TIME_VALID:
            /* Self-heal complete (SNTP synced after reconnect).
             * Only closes the AP in OPEN_INDEFINITE — not OPEN_TIMED. This is
             * intentional: when POST /time transitions OPEN_INDEFINITE→OPEN_TIMED,
             * EVT_POST_TIME is processed first (FIFO) and the state is already
             * OPEN_TIMED by the time this event arrives, so the AP is preserved.
             * See the comment in time_event_handler for the full ordering argument.
             *
             * Connected-but-no-SNTP: if STA connects but SNTP never succeeds
             * (router up, no internet), OPEN_INDEFINITE remains open — deliberate.
             * The AP gives the user a way to set time manually (POST /time), which
             * is exactly the §2 "time-is-the-gate" fallback. */
            if (s_ap_state == AP_OPEN_INDEFINITE) {
                ap_close();
            }
            break;
        case EVT_POST_TIME:
            /* User set time on prov page — transition OPEN_INDEFINITE → OPEN_TIMED */
            if (s_ap_state == AP_OPEN_INDEFINITE) {
                ap_open_timed();
            }
            break;
        case EVT_POST_CONNECT:
            /* Credentials saved — reboot after 3s so TCP can flush success.html */
            esp_timer_start_once(s_reboot_timer, REBOOT_DELAY_US);
            break;
        case EVT_TIMEOUT:
            if (s_ap_state == AP_OPEN_TIMED) {
                ap_close();
            }
            break;
        case EVT_BUTTON:
            /* Button press: open timed window (no-op if already OPEN_TIMED) */
            if (s_ap_state == AP_CLOSED || s_ap_state == AP_OPEN_INDEFINITE) {
                ap_open_timed();
            }
            break;
        default:
            break;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    /* sf_wifi_prov is the sole owner of SNTP retry (DESIGN.md §4.2) */
    if (id == SF_WIFI_EVENT_CONNECTED) {
        sf_time_sntp_restart();
    }
}

static void time_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    /* Re-post as EVT_TIME_VALID rather than calling ap_close() directly.
     * This extra hop is load-bearing for the POST /time race:
     *   post_time_handler calls sf_time_set_manual() → SF_TIME_EVENT_VALID is
     *   enqueued, then posts EVT_POST_TIME.  Because the event loop is a FIFO,
     *   EVT_POST_TIME (→ OPEN_TIMED) is dispatched before EVT_TIME_VALID lands
     *   here.  By the time EVT_TIME_VALID reaches prov_state_handler the state
     *   is already OPEN_TIMED, so the AP is NOT closed (§6.1 correct behaviour).
     *   A direct ap_close() call here would bypass that ordering and close the AP
     *   before EVT_POST_TIME is processed. Do NOT simplify. */
    esp_event_post(SF_PROV_EVENT, EVT_TIME_VALID, NULL, 0, pdMS_TO_TICKS(100));
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

sf_err_t sf_wifi_prov_init(const sf_wifi_prov_config_t *config)
{
    SF_CHECK_NULL_RETURN_ERR(ESP_LOGE, TAG, config, "config is NULL");
    s_config = *config;

    /* Creates the default event loop — must happen before handler registration. */
    esp_err_t err;

    err = sf_wifi_driver_init();
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "sf_wifi_driver_init: %d", err);

    /* Timers */
    esp_timer_create_args_t reboot_args = {
        .callback = reboot_timer_cb,
        .name     = "prov_reboot",
    };
    err = esp_timer_create(&reboot_args, &s_reboot_timer);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "reboot timer create: %d", err);

    esp_timer_create_args_t timeout_args = {
        .callback = timeout_timer_cb,
        .name     = "prov_timeout",
    };
    err = esp_timer_create(&timeout_args, &s_timeout_timer);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "timeout timer create: %d", err);

    /* Event handlers — registered after the event loop exists */
    err = esp_event_handler_register(SF_PROV_EVENT, ESP_EVENT_ANY_ID,
                                     prov_state_handler, NULL);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "Register SF_PROV_EVENT handler: %d", err);

    err = esp_event_handler_register(SF_WIFI_EVENT, SF_WIFI_EVENT_CONNECTED,
                                     wifi_event_handler, NULL);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "Register SF_WIFI_EVENT_CONNECTED handler: %d", err);

    err = esp_event_handler_register(SF_TIME_EVENT, SF_TIME_EVENT_VALID,
                                     time_event_handler, NULL);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, err != ESP_OK, SF_FAIL,
                             "Register SF_TIME_EVENT_VALID handler: %d", err);

    /* WDT note: sf_wifi_connect() NO_AP path blocks ~25-30 s (5 retries × ~5 s).
     * The task yields inside xEventGroupWaitBits(), so the idle-task WDT is fed
     * normally. Risk only exists if the init task is explicitly subscribed via
     * esp_task_wdt_add() — which it must not be during this call.
     * Fix (caller-side, Commit 5): sf_watering calls esp_task_wdt_add(NULL) AFTER
     * sf_wifi_prov_init() returns, never before. AUTH_FAIL returns immediately
     * (0 retries) so the slow path is NO_AP-at-boot only. See DESIGN.md §12. */

    /* Initial connect attempt */
    char ssid[MAX_SSID_LEN]     = {0};
    char password[MAX_PASS_LEN] = {0};
    bool connected = false;
    
    bool has_creds = nvs_read_creds(ssid, sizeof(ssid), password, sizeof(password));

    
    if (has_creds) {
        sf_wifi_conn_status_t result = sf_wifi_connect(ssid, password);
        connected = (result == SF_WIFI_CONN_OK);
        if (!connected) {
            ESP_LOGW(TAG, "Initial connect failed (status %d)", result);
        }
    }

    /* Single SoftAP evaluation — covers no-creds and connect-failed paths.
     * Called directly (not via prov_state_handler) — deliberate exception to the
     * §6.5 "all transitions on the loop" rule: SF_WIFI_EVENT is suppressed during
     * the blocking connect above, SNTP hasn't started, and there is no HTTP server
     * yet, so no concurrent transition can race this call. */
    if (!connected && !sf_time_is_valid()) {
        ap_open_indefinite();
    }

    ESP_LOGI(TAG, "sf_wifi_prov_init OK (connected=%d)", connected);
    return SF_OK;
}

sf_err_t sf_wifi_prov_start_server(void)
{
    /* Called from sf_watering button handler (task context — safe to post) */
    esp_event_post(SF_PROV_EVENT, EVT_BUTTON, NULL, 0, pdMS_TO_TICKS(100));
    return SF_OK;
}

sf_err_t sf_wifi_prov_clear(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGE, TAG, err, "nvs_open: %d", err);
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Credentials cleared");
    return SF_OK;
}
