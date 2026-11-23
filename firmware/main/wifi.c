#include "wifi.h"

#include <stdio.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "secretos.h"

_Static_assert(sizeof WIFI_SSID <= 33 && sizeof WIFI_PASS <= 65, "SSID máx. 32 y contraseña máx. 64 caracteres");

#define BIT_IP BIT0 // la "bandera" que levanta el handler al recibir IP
// Calibración: potencia en unidades de 0.25 dBm (34 = 8.5 dBm; máx. 80 = 20 dBm).
// Si sigue fallando prueba 28 (7 dBm); si conecta pero la señal es débil, sube de a poco.
#define POTENCIA_TX 34

static const char *TAG = "wifi";
static EventGroupHandle_t eventos;
static char ip[16]; // "255.255.255.255" + '\0'

// Lo llama ESP-IDF en cada evento de WiFi o IP. `data` depende del evento.
static void al_evento(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        // 15 = contraseña incorrecta, 201 = red no encontrada (¿es de 2.4 GHz?)
        ESP_LOGW(TAG, "desconectado (motivo %d), reintentando", d->reason);
        ip[0] = 0;
        xEventGroupClearBits(eventos, BIT_IP);
        esp_wifi_connect(); // ponytail: reintento inmediato y sin fin; agregar espera creciente si satura el log
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *d = data;
        snprintf(ip, sizeof ip, IPSTR, IP2STR(&d->ip_info.ip));
        ESP_LOGI(TAG, "conectado, IP %s", ip);
        xEventGroupSetBits(eventos, BIT_IP);
    }
}

static esp_err_t iniciar_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "borrar NVS"); // partición llena o de otra versión de IDF
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t wifi_conectar(int espera_ms) {
    eventos = xEventGroupCreate();
    ESP_RETURN_ON_ERROR(iniciar_nvs(), TAG, "NVS"); // el driver de WiFi guarda ahí la calibración de la radio
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");            // pila TCP/IP (lwIP)
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "eventos");
    esp_netif_create_default_wifi_sta();                            // interfaz de red + cliente DHCP

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, al_evento, NULL, NULL), TAG, "handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, al_evento, NULL, NULL), TAG, "handler");

    wifi_config_t cfg = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "modo"); // estación = cliente de un router
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start"); // → evento STA_START → esp_wifi_connect()
    // La antena del C3 SuperMini está mal ajustada: a potencia máxima el router no entiende la señal
    // (falla con "auth -> init (0x200)"). Debe ir después de esp_wifi_start().
    ESP_RETURN_ON_ERROR(esp_wifi_set_max_tx_power(POTENCIA_TX), TAG, "potencia");

    EventBits_t bits = xEventGroupWaitBits(eventos, BIT_IP, pdFALSE, pdTRUE, pdMS_TO_TICKS(espera_ms));
    return bits & BIT_IP ? ESP_OK : ESP_ERR_TIMEOUT;
}

const char *wifi_ip(void) { return ip; }
