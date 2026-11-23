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
    texto_dibujar(fb, "aaaaaaaaaaaaaaaaaaaa bb"); // 20 'a' + espacio: "bb" no cabe en la columna 21
    assert(glifo_en(0, 19, 'a') && glifo_en(1, 0, 'b') && glifo_en(1, 1, 'b'));

    // UTF-8: "é" son 2 bytes pero 1 letra; Á se dibuja como A.
    texto_dibujar(fb, "¿Qué? Á");
    assert(glifo_en(0, 0, 0xBF) && glifo_en(0, 3, 0xE9) && glifo_en(0, 4, '?') && glifo_en(0, 6, 'A'));

    // '\n' fuerza salto; una palabra más larga que una línea se corta.
    texto_dibujar(fb, "a\nbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(glifo_en(1, 0, 'b') && glifo_en(2, 0, 'b') && glifo_en(2, 3, 'b'));

    // Texto mayor que la pantalla: no escribe fuera del framebuffer (lo vigila -fsanitize=address).
    char largo[400];
    memset(largo, 'x', sizeof largo - 1), largo[sizeof largo - 1] = 0;
    texto_dibujar(fb, largo);

    texto_dibujar(fb, "Escuchando...\n\n¿Qué hora es? Son las 3:45 de la tarde, señor. ¡Ñandú!");
    imprimir();
    puts("ok");
}
