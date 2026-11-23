#include "hora.h"

#include <time.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "hora";

esp_err_t hora_sincronizar(int espera_ms) {
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&cfg), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_netif_sntp_sync_wait(pdMS_TO_TICKS(espera_ms)), TAG, "sin respuesta de pool.ntp.org");

    char texto[32];
    time_t ahora = time(NULL);
    struct tm utc;
    strftime(texto, sizeof texto, "%Y-%m-%d %H:%M:%S", gmtime_r(&ahora, &utc));
    ESP_LOGI(TAG, "%s UTC", texto); // el reloj sigue solo; SNTP lo corrige cada hora
    return ESP_OK;
}
