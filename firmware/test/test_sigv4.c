// Prueba de sigv4.c en la Mac, con CommonCrypto en lugar de mbedTLS:
//   cc -Wno-deprecated-declarations -fsanitize=address -I main test/test_sigv4.c main/sigv4.c -o /tmp/test_sigv4 && /tmp/test_sigv4
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sigv4.h"

void sigv4_sha256(const void *datos, size_t n, uint8_t salida[32]) { CC_SHA256(datos, (CC_LONG)n, salida); }
void sigv4_hmac(const uint8_t *llave, size_t nllave, const void *datos, size_t n, uint8_t salida[32]) {
    CCHmac(kCCHmacAlgSHA256, llave, nllave, datos, n, salida);
}

static const char *firma(const char *url) { return strstr(url, "X-Amz-Signature=") + 16; }

int main(void) {
    char url[4096];

    // 1. Ejemplo oficial de AWS (S3, "Authenticating Requests: Using Query Parameters").
    sigv4_t s3 = {
        .access_key = "AKIAIOSFODNN7EXAMPLE", .secret_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
        .region = "us-east-1", .servicio = "s3", .host = "examplebucket.s3.amazonaws.com", .esquema = "https",
        .ruta = "/test.txt", .fecha = "20130524T000000Z", .expira = 86400,
    };
    assert(sigv4_prefirmar(url, sizeof url, &s3) > 0);
    assert(strcmp(firma(url), "aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404") == 0);

    // 2. AgentCore, contra botocore (generado con test/vector_sigv4.py): token, ARN codificado, session id.
    sigv4_t ac = {
        .access_key = "ASIAEJEMPLO0000000000", .secret_key = "secretoFalso/0123456789+abcdefghijklmnopqr",
        .token = "TokenFalso//con+simbolos=yMas==", .region = "us-east-1", .servicio = "bedrock-agentcore",
        .host = "bedrock-agentcore.us-east-1.amazonaws.com", .esquema = "wss",
        .ruta = "/runtimes/arn%3Aaws%3Abedrock-agentcore%3Aus-east-1%3A123456789012%3Aruntime%2Fasistente-AbCdE12345/ws",
        .extra_clave = "X-Amzn-Bedrock-AgentCore-Runtime-Session-Id", .extra_valor = "sesion-de-prueba-0123456789abcdef0123",
        .fecha = "20261123T030405Z", .expira = 300,
    };
    assert(sigv4_prefirmar(url, sizeof url, &ac) > 0);
    assert(strcmp(firma(url), "a5ce0cf911dc634b94da844b760e969e53555116719703a10088bea223b0b05c") == 0);
    assert(strstr(url, "X-Amz-Security-Token=TokenFalso%2F%2Fcon%2Bsimbolos%3DyMas%3D%3D"));

    // 3. Si no cabe, avisa en vez de cortar la URL.
    assert(sigv4_prefirmar(url, 100, &ac) == -1);
    puts("ok");
}
