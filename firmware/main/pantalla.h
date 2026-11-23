// OLED SSD1306 128×64 por I2C.
#pragma once
#include "esp_err.h"

#include <stdbool.h>

// Fila 0: barra de estado (título a la izquierda, íconos de conexión a la derecha).
// Filas 1-7: contenido, con salto de línea automático.
esp_err_t pantalla_iniciar(void);
void pantalla_mostrar(const char *titulo, const char *contenido);
// Íconos de conexión: visibles si hay conexión. Redibuja solo si cambiaron.
void pantalla_estado(bool wifi, bool agente);
