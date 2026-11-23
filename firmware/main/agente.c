#include "agente.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "credenciales.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "sigv4.h"

#define HOST "bedrock-agentcore.us-east-1.amazonaws.com"
#define RENOVAR_SEG (50 * 60) // las credenciales duran 1 h: se renuevan antes
#define BIT_CONECTADO BIT0

extern const char CA[] asm("_binary_AmazonRootCA1_pem_start");       // también verifica a AgentCore
extern const char ARN[] asm("_binary_agente_arn_txt_start");         // lo escribe infra/certs.sh

static const char *TAG = "agente";
static esp_websocket_client_handle_t ws;
static QueueHandle_t mensajes;   // tarea del WebSocket → app_main (copia segura entre tareas)
static EventGroupHandle_t estado;
static credenciales_t cred;      // static: 2.2 KB
static time_t cred_desde;        // cuándo se obtuvieron (0 = nunca)
static char sesion[48];          // mismo id = misma microVM = el agente recuerda la conversación

// Corre en la tarea del cliente WebSocket, no en app_main.
static void al_evento(void *arg, esp_event_base_t base, int32_t id, void *datos) {
    esp_websocket_event_data_t *d = datos;
    if (id == WEBSOCKET_EVENT_CONNECTED) {
        xEventGroupSetBits(estado, BIT_CONECTADO);
    } else if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED) {
        xEventGroupClearBits(estado, BIT_CONECTADO);
        ESP_LOGW(TAG, "desconectado (handshake HTTP %d)", d->error_handle.esp_ws_handshake_status_code);
    } else if (id == WEBSOCKET_EVENT_DATA && d->op_code == 0x1) { // 0x1 = frame de texto (nuestro JSON)
        if (d->payload_offset != 0 || d->data_len != d->payload_len) return; // fragmentado: no esperado
        cJSON *json = cJSON_ParseWithLength(d->data_ptr, d->data_len);
        const char *tipo = cJSON_GetStringValue(cJSON_GetObjectItem(json, "tipo"));
        const char *texto = cJSON_GetStringValue(cJSON_GetObjectItem(json, "texto"));
        if (tipo && texto) {
            agente_mensaje_t m = {.tipo = tipo[0] == 't' ? 't' : tipo[0] == 'r' ? 'r' : 'e'};
            snprintf(m.texto, sizeof m.texto, "%s", texto); // se corta si es más largo (cabe en la OLED)
            xQueueSend(mensajes, &m, 0);                     // 0 = si la cola está llena, se descarta
        }
        cJSON_Delete(json);
    }
}

static esp_err_t firmar_url(char *url, size_t max) {
    static char ruta[256], arn[160]; // static: la pila de main es de ~4 KB
    char fecha[17];
    ESP_RETURN_ON_FALSE(sigv4_codificar(arn, sizeof arn, ARN, 1) > 0, ESP_ERR_INVALID_SIZE, TAG, "ARN");
    snprintf(ruta, sizeof ruta, "/runtimes/%s/ws", arn);
    time_t ahora = time(NULL);
    struct tm utc;
    strftime(fecha, sizeof fecha, "%Y%m%dT%H%M%SZ", gmtime_r(&ahora, &utc)); // por eso hace falta SNTP
    sigv4_t p = {
        .access_key = cred.access_key, .secret_key = cred.secret_key, .token = cred.token,
        .region = "us-east-1", .servicio = "bedrock-agentcore", .host = HOST, .esquema = "wss", .ruta = ruta,
        .extra_clave = "X-Amzn-Bedrock-AgentCore-Runtime-Session-Id", .extra_valor = sesion,
        .fecha = fecha, .expira = 300, // solo tiene que valer al conectar
    };
    ESP_RETURN_ON_FALSE(sigv4_prefirmar(url, max, &p) > 0, ESP_ERR_INVALID_SIZE, TAG, "URL");
    return ESP_OK;
}

esp_err_t agente_conectar(void) {
    static char url[2560]; // ~1.6 KB con el token
    if (!mensajes) {
        mensajes = xQueueCreate(4, sizeof(agente_mensaje_t));
        estado = xEventGroupCreate();
        snprintf(sesion, sizeof sesion, "remi-c3-%08lx%08lx%08lx", esp_random(), esp_random(), esp_random());
    }
    if (ws) { // la conexión anterior se descarta: su URL ya caducó
        esp_websocket_client_destroy(ws);
        ws = NULL;
    }
    if (!cred_desde || time(NULL) - cred_desde > RENOVAR_SEG) {
        ESP_RETURN_ON_ERROR(credenciales_obtener(&cred), TAG, "credenciales");
        cred_desde = time(NULL);
    }
    ESP_RETURN_ON_ERROR(firmar_url(url, sizeof url), TAG, "firmar");

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .cert_pem = CA,
        .disable_auto_reconnect = true, // reconectar requiere una URL nueva: lo hace agente_conectar()
        .task_stack = 6144,             // cJSON en el handler necesita más pila que el default
        .network_timeout_ms = 10000,
    };
    ws = esp_websocket_client_init(&cfg);
    ESP_RETURN_ON_FALSE(ws, ESP_ERR_NO_MEM, TAG, "init");
    esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, al_evento, NULL);
    ESP_RETURN_ON_ERROR(esp_websocket_client_start(ws), TAG, "start");
    EventBits_t b = xEventGroupWaitBits(estado, BIT_CONECTADO, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    if (b & BIT_CONECTADO) ESP_LOGI(TAG, "conectado a AgentCore");
    return b & BIT_CONECTADO ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool agente_conectado(void) { return estado && (xEventGroupGetBits(estado) & BIT_CONECTADO); }

esp_err_t agente_enviar_audio(const int16_t *pcm, size_t n) {
    int r = esp_websocket_client_send_bin(ws, (const char *)pcm, n * sizeof pcm[0], pdMS_TO_TICKS(1000));
    return r < 0 ? ESP_FAIL : ESP_OK;
}

esp_err_t agente_fin(void) {
    return esp_websocket_client_send_text(ws, "fin", 3, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK;
}

bool agente_mensaje(agente_mensaje_t *m) { return mensajes && xQueueReceive(mensajes, m, 0) == pdTRUE; }
