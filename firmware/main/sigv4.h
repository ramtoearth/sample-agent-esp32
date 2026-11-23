// URL prefirmada con AWS SigV4 (firma en la URL, válida `expira` segundos). Sin dependencias de
// hardware: se prueba en la laptop (test/test_sigv4.c) contra AWS y botocore.
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *access_key, *secret_key, *token; // token: NULL si son credenciales permanentes
    const char *region, *servicio, *host;
    const char *esquema; // "wss" o "https"
    const char *ruta;    // ya codificada como va en la URL, ej. "/runtimes/arn%3Aaws%3A.../ws"
    const char *extra_clave, *extra_valor; // un parámetro adicional (NULL si no hay)
    const char *fecha;   // "AAAAMMDDTHHMMSSZ" en UTC
    int expira;          // segundos (AgentCore: máx. 300)
} sigv4_t;

// Codifica `s` como exige SigV4 (también '/' si `barra`). Devuelve el largo, o -1 si no cabe.
int sigv4_codificar(char *dst, size_t max, const char *s, int barra);

// Escribe la URL en `url`. Devuelve su largo, o -1 si no cabe.
int sigv4_prefirmar(char *url, size_t max, const sigv4_t *p);

// Las aporta cada plataforma: mbedTLS en el ESP32 (sigv4_mbedtls.c), CommonCrypto en la Mac (test).
void sigv4_sha256(const void *datos, size_t n, uint8_t salida[32]);
void sigv4_hmac(const uint8_t *llave, size_t nllave, const void *datos, size_t n, uint8_t salida[32]);
