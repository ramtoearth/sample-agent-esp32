// sha256 y hmac de sigv4.c con mbedTLS (ya está en el binario por TLS; usa el acelerador SHA del C3).
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "sigv4.h"

void sigv4_sha256(const void *datos, size_t n, uint8_t salida[32]) { mbedtls_sha256(datos, n, salida, 0); }

void sigv4_hmac(const uint8_t *llave, size_t nllave, const void *datos, size_t n, uint8_t salida[32]) {
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), llave, nllave, datos, n, salida);
}
