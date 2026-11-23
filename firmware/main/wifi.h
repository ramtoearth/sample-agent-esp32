// WiFi en modo estación (cliente) con reconexión automática. Datos de la red en secretos.h.
#pragma once
#include "esp_err.h"

// Conecta y espera hasta tener IP o hasta `espera_ms`. Si se agota el tiempo devuelve
// ESP_ERR_TIMEOUT, pero sigue reintentando en segundo plano.
esp_err_t wifi_conectar(int espera_ms);
const char *wifi_ip(void); // "192.168.x.x", o "" si no hay conexión
