// OLED SSD1306 128×64 por I2C.
#pragma once
#include "esp_err.h"

esp_err_t pantalla_iniciar(void);
void pantalla_mostrar(const char *utf8); // borra y escribe; salto de línea automático
