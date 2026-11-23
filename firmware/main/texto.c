#include "texto.h"

#include <string.h>

#include "fuente.h"

_Static_assert(sizeof FUENTE_ASCII / sizeof FUENTE_ASCII[0] == 0x7E - 0x20 + 1, "faltan glifos ASCII");

static int es_continuacion(char c) { return ((uint8_t)c & 0xC0) == 0x80; } // bytes 10xxxxxx de UTF-8

// Lee un carácter UTF-8 y avanza el puntero. Solo hacen falta las secuencias de 2 bytes
// (á, ñ, ¿…); las de 3-4 (emojis) se reemplazan por '?'.
static uint32_t leer_utf8(const char **s) {
    const uint8_t *p = (const uint8_t *)*s;
    if (p[0] < 0x80) {
        *s += 1;
        return p[0];
    }
    if ((p[0] & 0xE0) == 0xC0 && es_continuacion(p[1])) {
        *s += 2;
        return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    }
    do (*s)++; while (es_continuacion(**s));
    return '?';
}

void texto_dibujar(uint8_t fb[TEXTO_FB_BYTES], const char *s) {
    memset(fb, 0, TEXTO_FB_BYTES);
    int fila = 0, col = 0;
    while (*s && fila < TEXTO_FILAS) {
        if (*s == '\n') {
            fila++, col = 0, s++;
        } else if (*s == ' ') {
            if (col > 0 && ++col >= TEXTO_COLS) fila++, col = 0; // sin espacios al inicio de línea
            s++;
        } else {
            const char *fin = s;
            int largo = 0;
            for (; *fin && *fin != ' ' && *fin != '\n'; fin++) largo += !es_continuacion(*fin);
            if (col > 0 && col + largo > TEXTO_COLS) fila++, col = 0; // la palabra no cabe: siguiente línea
            while (s < fin && fila < TEXTO_FILAS) {
                memcpy(&fb[fila * TEXTO_ANCHO + col * 6], fuente_glifo(leer_utf8(&s)), 5);
                if (++col == TEXTO_COLS) fila++, col = 0; // palabra más larga que una línea: se corta
            }
        }
    }
}
