#include "web.h"
#include "shared.h"
#include "web_ui.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "nvs_flash.h"

#include <string.h>
#include <stdio.h>

static const char* TAG = "web";

// ---------------------------------------------------------------------------
// WiFi AP
// ---------------------------------------------------------------------------

static void wifi_init_ap(void) {
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len      = strlen(AP_SSID);
    ap_config.ap.channel       = 1;
    ap_config.ap.authmode      = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 4;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi AP started: SSID=%s  IP=%s", AP_SSID, AP_IP);
}

// ---------------------------------------------------------------------------
// DNS captive portal — responds to all A queries with AP_IP (192.168.4.1)
// ---------------------------------------------------------------------------

static void dns_task(void* arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(53);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    uint8_t buf[512];
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    while (true) {
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr*)&client, &client_len);
        if (len < 12) continue;

        // Build response: copy header, set QR+RA flags, set answer count = 1
        uint8_t resp[512];
        memcpy(resp, buf, len);
        resp[2] = 0x81;   // QR=1, Opcode=0, AA=1, TC=0, RD=1
        resp[3] = 0x80;   // RA=1, no error
        resp[6] = 0x00;   // answer count high
        resp[7] = 0x01;   // answer count low = 1

        // Append answer RR: pointer to question name, A record, 192.168.4.1
        int pos = len;
        resp[pos++] = 0xC0; resp[pos++] = 0x0C;  // name pointer → offset 12
        resp[pos++] = 0x00; resp[pos++] = 0x01;   // type A
        resp[pos++] = 0x00; resp[pos++] = 0x01;   // class IN
        resp[pos++] = 0x00; resp[pos++] = 0x00;
        resp[pos++] = 0x00; resp[pos++] = 0x3C;   // TTL 60s
        resp[pos++] = 0x00; resp[pos++] = 0x04;   // rdlength 4
        resp[pos++] = 192;  resp[pos++] = 168;
        resp[pos++] = 4;    resp[pos++] = 1;       // 192.168.4.1

        sendto(sock, resp, pos, 0,
               (struct sockaddr*)&client, client_len);
    }
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------

// GET / — serve web UI
static esp_err_t handle_root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, WEB_UI_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GET /api/status — JSON snapshot
static esp_err_t handle_status(httpd_req_t* req) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"rpm\":%lu,\"synced\":%s,\"offset_tenths\":%d,"
             "\"switch_mode\":%s,\"teeth\":%u}",
             (unsigned long)get_rpm(),
             get_synced() ? "true" : "false",
             get_offset_tenths(),
             get_switch_mode() ? "true" : "false",
             get_teeth_total());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/offset?value=X  (X = tenths, -100..+100)
static esp_err_t handle_offset(httpd_req_t* req) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(query, "value", val, sizeof(val)) == ESP_OK) {
            set_offset_tenths((int16_t)atoi(val));
        }
    }
    httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// POST /api/config?switch_mode=0|1&teeth=N
static esp_err_t handle_config(httpd_req_t* req) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(query, "switch_mode", val, sizeof(val)) == ESP_OK)
            set_switch_mode(atoi(val) != 0);
        if (httpd_query_key_value(query, "teeth", val, sizeof(val)) == ESP_OK)
            set_teeth_total((uint8_t)atoi(val));
    }
    httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Catch-all → redirect to / (captive portal)
static esp_err_t handle_catchall(httpd_req_t* req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// HTTP server init
// ---------------------------------------------------------------------------

static void http_server_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t uris[] = {
        { "/",           HTTP_GET,  handle_root,     NULL },
        { "/api/status", HTTP_GET,  handle_status,   NULL },
        { "/api/offset", HTTP_POST, handle_offset,   NULL },
        { "/api/config", HTTP_POST, handle_config,   NULL },
        { "/*",          HTTP_GET,  handle_catchall,  NULL },
    };
    for (auto& u : uris)
        httpd_register_uri_handler(server, &u);

    ESP_LOGI(TAG, "HTTP server started");
}

// ---------------------------------------------------------------------------
// Web task (pinned to Core 0)
// ---------------------------------------------------------------------------

void web_task(void* arg) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_ap();
    http_server_start();

    // DNS captive portal runs in its own small task (still Core 0)
    xTaskCreatePinnedToCore(dns_task, "dns", 4096, NULL, 4, NULL, 0);

    // Nothing left to do — HTTP server and DNS are event-driven
    vTaskDelete(NULL);
}
