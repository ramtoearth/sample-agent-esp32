// Hora real por SNTP. SigV4 (paso 4d) la necesita: AWS rechaza firmas con más de 5 min de diferencia.
#pragma once
#include "esp_err.h"

esp_err_t hora_sincronizar(int espera_ms); // requiere WiFi conectado
