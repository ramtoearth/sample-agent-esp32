#include "credenciales.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define THING "asistente-c3"               // = THING en infra/app.py
#define ROLE_ALIAS "asistente-dispositivo" // = ROLE_ALIAS en infra/app.py

// Archivos de firmware/certs/ incrustados al compilar (EMBED_TXTFILES, terminan en '\0').
// El linker los nombra _binary_<archivo>_start; asm("...") enlaza nuestro nombre de C con ese símbolo.
extern const char CA[] asm("_binary_AmazonRootCA1_pem_start");    // para verificar que el servidor es AWS
extern const char CERT[] asm("_binary_dispositivo_crt_start");     // nuestra identidad
extern const char LLAVE[] asm("_binary_dispositivo_key_start");    // prueba de que el certificado es nuestro
extern const char ENDPOINT[] asm("_binary_endpoint_txt_start");    // lo escribe infra/certs.sh

static const char *TAG = "credenciales";

// Copia un campo de texto del JSON validando que exista y que quepa.
static esp_err_t copiar(char *destino, size_t max, const cJSON *obj, const char *campo) {
    const char *v = cJSON_GetStringValue(cJSON_GetObjectItem(obj, campo));
    if (!v || strlen(v) >= max) return ESP_ERR_INVALID_RESPONSE;
    strcpy(destino, v); // seguro: acabamos de comprobar el largo
    return ESP_OK;
}

esp_err_t credenciales_obtener(credenciales_t *c) {
    static char respuesta[3072]; // static: no cabe en la pila; el JSON mide ~1.5 KB
    char url[128];
    snprintf(url, sizeof url, "https://%s/role-aliases/%s/credentials", ENDPOINT, ROLE_ALIAS);

    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = CA,
        .client_cert_pem = CERT, // TLS mutuo: el cliente también presenta certificado
        .client_key_pem = LLAVE,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    if (!http) return ESP_ERR_NO_MEM;
    esp_http_client_set_header(http, "x-amzn-iot-thingname", THING); // IoT Core verifica que el cert sea de este Thing

    int estado = 0, largo = 0;
    esp_err_t err = esp_http_client_open(http, 0); // 0 = GET sin cuerpo
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(http);
        estado = esp_http_client_get_status_code(http);
        largo = esp_http_client_read_response(http, respuesta, sizeof respuesta - 1);
    }
    esp_http_client_cleanup(http); // siempre: cierra la conexión y libera memoria, también si falló
    ESP_RETURN_ON_ERROR(err, TAG, "conexión TLS");
    if (largo < 0 || largo >= (int)sizeof respuesta - 1) return ESP_ERR_INVALID_SIZE; // error o no cupo completa
    respuesta[largo] = 0;
    if (estado != 200) { // el cuerpo es un mensaje de error de AWS, sin credenciales: se puede mostrar
        ESP_LOGE(TAG, "HTTP %d: %s", estado, respuesta);
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(respuesta);
    const cJSON *cred = cJSON_GetObjectItem(json, "credentials"); // si json es NULL, devuelve NULL
    err = cred ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
    if (err == ESP_OK) err = copiar(c->access_key, sizeof c->access_key, cred, "accessKeyId");
    if (err == ESP_OK) err = copiar(c->secret_key, sizeof c->secret_key, cred, "secretAccessKey");
    if (err == ESP_OK) err = copiar(c->token, sizeof c->token, cred, "sessionToken");
    if (err == ESP_OK) err = copiar(c->expira, sizeof c->expira, cred, "expiration");
    cJSON_Delete(json);                        // cJSON usa malloc: hay que liberar
    memset(respuesta, 0, sizeof respuesta);    // no dejar las credenciales en un buffer que ya no usamos
    if (err == ESP_OK) ESP_LOGI(TAG, "credenciales temporales OK, vencen %s", c->expira);
    return err;
}
