idf_component_register(SRCS "main.c" "pantalla.c" "texto.c" "mic.c" "audio.c"
                    PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_usb_serial_jtag
                    INCLUDE_DIRS "")
