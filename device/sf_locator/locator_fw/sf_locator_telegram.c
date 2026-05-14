#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "sf_err.h"
#include "sf_locator_telegram.h"
#include "config.h"

static const char *TAG = "SF_LOCATOR_TG";

#define TG_URL_BUF_LEN  256
#define TG_BODY_BUF_LEN 1024

static bool wifi_connected(void)
{
    wifi_ap_record_t info;
    return (esp_wifi_sta_get_ap_info(&info) == ESP_OK);
}

static void json_escape_into(const char *in, char *out, size_t out_len)
{
    size_t o = 0;
    if (out_len == 0) {
        return;
    }
    for (size_t i = 0; in[i] != '\0' && o + 2 < out_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (o + 2 >= out_len) break;
            out[o++] = '\\';
            out[o++] = c;
        } else if (c == '\n') {
            if (o + 2 >= out_len) break;
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            if (o + 2 >= out_len) break;
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c < 0x20) {
            /* skip other control chars */
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

sf_err_t sf_locator_telegram_send(const char *message)
{
    sf_err_t status = SF_FAIL;
    esp_http_client_handle_t client = NULL;
    char url[TG_URL_BUF_LEN];
    char body[TG_BODY_BUF_LEN];
    char escaped[TG_BODY_BUF_LEN - 64];

    if (message == NULL) {
        goto end;
    }
    if (BOT_TOKEN[0] == '\0' || CHAT_ID[0] == '\0') {
        ESP_LOGD(TAG, "BOT_TOKEN/CHAT_ID empty - skip send");
        status = SF_OK;
        goto end;
    }
    if (!wifi_connected()) {
        ESP_LOGD(TAG, "WiFi not connected - skip send");
        status = SF_OK;
        goto end;
    }

    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/sendMessage", BOT_TOKEN);

    json_escape_into(message, escaped, sizeof(escaped));
    snprintf(body, sizeof(body),
             "{\"chat_id\":\"%s\",\"text\":\"%s\"}", CHAT_ID, escaped);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    client = esp_http_client_init(&cfg);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, client, end, "esp_http_client_init returned NULL");

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "Telegram HTTP perform with status: %d", err);

    int code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Telegram sent (HTTP %d): %s", code, message);

    status = (code >= 200 && code < 300) ? SF_OK : SF_FAIL;
end:
    if (client) {
        esp_http_client_cleanup(client);
    }
    return status;
}
