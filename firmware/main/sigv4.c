#include "sigv4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Todo menos A-Z a-z 0-9 - _ . ~ se vuelve %XX ('/' solo si `barra`).
int sigv4_codificar(char *dst, size_t max, const char *s, int barra) {
    static const char HEX[] = "0123456789ABCDEF";
    size_t n = 0;
    for (; *s; s++) {
        unsigned char c = *s;
        int libre = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                    c == '_' || c == '.' || c == '~' || (c == '/' && !barra);
        if (n + (libre ? 1 : 3) >= max) return -1;
        if (libre) {
            dst[n++] = c;
        } else {
            dst[n++] = '%', dst[n++] = HEX[c >> 4], dst[n++] = HEX[c & 0xF];
        }
    }
    dst[n] = 0;
    return n;
}

static void a_hex(const uint8_t *b, size_t n, char *hex) {
    for (size_t i = 0; i < n; i++) sprintf(hex + 2 * i, "%02x", b[i]);
}

static int comparar(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int sigv4_prefirmar(char *url, size_t max, const sigv4_t *p) {
    // ponytail: buffers fijos dimensionados para el token de IoT Core (~1 KB); no es reentrante.
    // En el ESP32 la tarea main tiene ~4 KB de pila: los buffers grandes NO pueden ser locales.
    static char pares[8][2048]; // "clave=valor" ya codificados
    static char consulta[4096], canonica[6144], valor[2048], ruta[512];
    char dia[9], alcance[128];
    int n = 0;
    memcpy(dia, p->fecha, 8), dia[8] = 0;
    snprintf(alcance, sizeof alcance, "%s/%s/%s/aws4_request", dia, p->region, p->servicio);

    // 1. Parámetros de la firma (+ el extra), cada uno "clave=valor" codificado, y ordenados.
    char credencial[192], expira[12];
    snprintf(credencial, sizeof credencial, "%s/%s", p->access_key, alcance);
    snprintf(expira, sizeof expira, "%d", p->expira);
    const char *kv[][2] = {
        {"X-Amz-Algorithm", "AWS4-HMAC-SHA256"}, {"X-Amz-Credential", credencial}, {"X-Amz-Date", p->fecha},
        {"X-Amz-Expires", expira},               {"X-Amz-SignedHeaders", "host"},
        {"X-Amz-Security-Token", p->token},      {p->extra_clave, p->extra_valor},
    };
    char *orden[8];
    for (size_t i = 0; i < sizeof kv / sizeof kv[0]; i++) {
        if (!kv[i][0] || !kv[i][1]) continue; // sin token o sin extra
        if (sigv4_codificar(valor, sizeof valor, kv[i][1], 1) < 0) return -1;
        if (snprintf(pares[n], sizeof pares[n], "%s=%s", kv[i][0], valor) >= (int)sizeof pares[n]) return -1;
        orden[n] = pares[n], n++;
    }
    qsort(orden, n, sizeof orden[0], comparar); // SigV4 exige orden alfabético (byte a byte)
    size_t largo = 0;
    for (int i = 0; i < n; i++) {
        int k = snprintf(consulta + largo, sizeof consulta - largo, "%s%s", i ? "&" : "", orden[i]);
        if (k < 0 || (largo += k) >= sizeof consulta) return -1;
    }

    // 2. Petición canónica. S3 es la excepción en dos reglas: no recodifica la ruta y no resume el cuerpo.
    //    El resto: la ruta se codifica OTRA vez (%3A → %253A) y va el SHA-256 del cuerpo (vacío en un GET).
    uint8_t h[32];
    char hex[65], texto[256];
    int s3 = strcmp(p->servicio, "s3") == 0;
    if (s3) {
        if (snprintf(ruta, sizeof ruta, "%s", p->ruta) >= (int)sizeof ruta) return -1;
        strcpy(hex, "UNSIGNED-PAYLOAD");
    } else {
        if (sigv4_codificar(ruta, sizeof ruta, p->ruta, 0) < 0) return -1;
        sigv4_sha256("", 0, h); // = e3b0c442...b855
        a_hex(h, 32, hex);
    }
    int k = snprintf(canonica, sizeof canonica, "GET\n%s\n%s\nhost:%s\n\nhost\n%s", ruta, consulta, p->host, hex);
    if (k < 0 || k >= (int)sizeof canonica) return -1;

    // 3. Texto a firmar: algoritmo, fecha, alcance y el SHA-256 de la petición canónica.
    sigv4_sha256(canonica, k, h);
    a_hex(h, 32, hex);
    snprintf(texto, sizeof texto, "AWS4-HMAC-SHA256\n%s\n%s\n%s", p->fecha, alcance, hex);

    // 4. Llave de firma: HMAC encadenado; solo vale ese día, región y servicio.
    char semilla[96];
    snprintf(semilla, sizeof semilla, "AWS4%s", p->secret_key);
    uint8_t llave[32];
    sigv4_hmac((const uint8_t *)semilla, strlen(semilla), dia, strlen(dia), llave);
    sigv4_hmac(llave, 32, p->region, strlen(p->region), llave);
    sigv4_hmac(llave, 32, p->servicio, strlen(p->servicio), llave);
    sigv4_hmac(llave, 32, "aws4_request", 12, llave);
    sigv4_hmac(llave, 32, texto, strlen(texto), h);
    a_hex(h, 32, hex);
    memset(semilla, 0, sizeof semilla), memset(llave, 0, sizeof llave); // no dejar el secreto en la pila

    k = snprintf(url, max, "%s://%s%s?%s&X-Amz-Signature=%s", p->esquema, p->host, p->ruta, consulta, hex);
    return k < 0 || k >= (int)max ? -1 : k;
}
