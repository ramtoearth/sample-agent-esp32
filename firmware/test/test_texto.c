// Prueba de texto.c en la laptop (sin placa):
//   cc -fsanitize=address -I main test/test_texto.c main/texto.c -o /tmp/test_texto && /tmp/test_texto
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fuente.h"
#include "texto.h"

static uint8_t fb[TEXTO_FB_BYTES];

static int glifo_en(int fila, int col, uint32_t cp) {
    return memcmp(&fb[fila * TEXTO_ANCHO + col * 6], fuente_glifo(cp), 5) == 0;
}

static void imprimir(void) { // la pantalla en ASCII: '#' = píxel encendido
    for (int y = 0; y < TEXTO_ALTO; y++) {
        for (int x = 0; x < TEXTO_ANCHO; x++) putchar(fb[(y / 8) * TEXTO_ANCHO + x] >> (y % 8) & 1 ? '#' : '.');
        putchar('\n');
    }
}

int main(void) {
    // Una palabra que no cabe en lo que queda de la línea pasa completa a la siguiente.
    texto_dibujar(fb, 0, "aaaaaaaaaaaaaaaaaaaa bb"); // 20 'a' + espacio: "bb" no cabe en la columna 21
    assert(glifo_en(0, 19, 'a') && glifo_en(1, 0, 'b') && glifo_en(1, 1, 'b'));

    // UTF-8: "é" son 2 bytes pero 1 letra; Á se dibuja como A.
    texto_dibujar(fb, 0, "¿Qué? Á");
    assert(glifo_en(0, 0, 0xBF) && glifo_en(0, 3, 0xE9) && glifo_en(0, 4, '?') && glifo_en(0, 6, 'A'));

    // '\n' fuerza salto; una palabra más larga que una línea se corta.
    texto_dibujar(fb, 0, "a\nbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(glifo_en(1, 0, 'b') && glifo_en(2, 0, 'b') && glifo_en(2, 3, 'b'));

    // Texto mayor que la pantalla: no escribe fuera del framebuffer (lo vigila -fsanitize=address).
    char largo[400];
    memset(largo, 'x', sizeof largo - 1), largo[sizeof largo - 1] = 0;
    texto_dibujar(fb, 0, largo);

    // Desde la fila 1: la fila 0 no se toca (es la barra de estado).
    memset(fb, 0xAA, TEXTO_ANCHO);
    texto_dibujar(fb, 1, "hola");
    assert(fb[0] == 0xAA && fb[TEXTO_ANCHO - 1] == 0xAA && glifo_en(1, 0, 'h'));

    // Una línea: corta a max_cols, no salta de línea y no borra lo de alrededor.
    texto_dibujar(fb, 0, "");
    texto_linea(fb, 0, 0, 3, "abcdef");
    assert(glifo_en(0, 2, 'c') && !glifo_en(0, 3, 'd') && !glifo_en(1, 0, 'd'));
    texto_linea(fb, 0, TEXTO_COLS - 1, 5, "xyz"); // no se pasa de la columna 20
    assert(glifo_en(0, TEXTO_COLS - 1, 'x'));

    // La pantalla como la arma pantalla.c: barra de estado + contenido desde la fila 1.
    texto_dibujar(fb, 1, "Tu: que hora es\n\nSon las 3:45 de la tarde, señor. ¡Ñandú!");
    memset(fb, 0, TEXTO_ANCHO);
    texto_linea(fb, 0, 0, TEXTO_COLS - 3, "Pensando...");
    texto_linea(fb, 0, TEXTO_COLS - 2, 1, TEXTO_ICONO_WIFI);
    texto_linea(fb, 0, TEXTO_COLS - 1, 1, TEXTO_ICONO_AGENTE);
    for (int x = 0; x < TEXTO_ANCHO; x++) fb[x] |= 0x80; // línea separadora
    imprimir();
    puts("ok");
}
