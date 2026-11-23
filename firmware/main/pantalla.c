#include "pantalla.h"

#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "texto.h"

#define PIN_SDA 21
#define PIN_SCL 20
// Calibración: según el módulo pueden cambiar.
#define DIRECCION 0x3C // impresa como 0x78 en la placa (= 0x3C << 1); algunos módulos usan 0x3D
#define ESPEJO_X false // ponlos en true si el texto sale volteado
#define ESPEJO_Y false

static const char *TAG = "pantalla";
static esp_lcd_panel_handle_t panel;
static uint8_t fb[TEXTO_FB_BYTES]; // copia de la memoria del SSD1306

esp_err_t pantalla_iniciar(void) {
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "bus I2C");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = { // valores de la hoja de datos del SSD1306
        .dev_addr = DIRECCION,
        .scl_speed_hz = 400 * 1000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6, // el bit D/C (comando o dato) va en el bit 6 del byte de control
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), TAG, "io I2C");

    esp_lcd_panel_ssd1306_config_t ssd_cfg = {.height = TEXTO_ALTO};
    esp_lcd_panel_dev_config_t panel_cfg = {.bits_per_pixel = 1, .reset_gpio_num = -1, .vendor_config = &ssd_cfg};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ssd1306(io, &panel_cfg, &panel), TAG, "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init (¿cableado o dirección 0x%02X?)", DIRECCION);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel, ESPEJO_X, ESPEJO_Y), TAG, "espejo");
    return esp_lcd_panel_disp_on_off(panel, true);
}

static char titulo[TEXTO_COLS * 2], contenido[320]; // copia de lo último mostrado (UTF-8: hasta 2 bytes por letra)
static bool con_wifi, con_agente;

static void dibujar(void) {
    texto_dibujar(fb, 1, contenido);
    memset(fb, 0, TEXTO_ANCHO); // fila 0 (la barra)
    texto_linea(fb, 0, 0, TEXTO_COLS - 3, titulo); // deja una columna de aire antes de los íconos
    if (con_wifi) texto_linea(fb, 0, TEXTO_COLS - 2, 1, TEXTO_ICONO_WIFI);
    if (con_agente) texto_linea(fb, 0, TEXTO_COLS - 1, 1, TEXTO_ICONO_AGENTE);
    for (int x = 0; x < TEXTO_ANCHO; x++) fb[x] |= 0x80; // línea separadora: el bit 7 de la fila 0 está libre
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_draw_bitmap(panel, 0, 0, TEXTO_ANCHO, TEXTO_ALTO, fb));
}

void pantalla_mostrar(const char *t, const char *c) {
    snprintf(titulo, sizeof titulo, "%s", t);
    snprintf(contenido, sizeof contenido, "%s", c);
    dibujar();
}

void pantalla_estado(bool wifi, bool agente) {
    if (wifi == con_wifi && agente == con_agente) return; // nada cambió: no gastar I2C
    con_wifi = wifi, con_agente = agente;
    dibujar();
}
