// Credenciales temporales de AWS (1 h) a cambio del certificado del dispositivo:
// HTTPS con TLS mutuo al credentials provider de IoT Core. Ver infra/app.py.
#pragma once
#include "esp_err.h"

typedef struct {
    char access_key[32];
    char secret_key[64];
    char token[2048];
    char expira[32]; // "2026-11-23T03:35:43Z"
} credenciales_t;

// Nunca imprimas los campos: el log sale por USB.
esp_err_t credenciales_obtener(credenciales_t *c);
