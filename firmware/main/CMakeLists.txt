# Como el build.rs del template: fallar pronto y con instrucciones si faltan los secretos.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/secretos.h")
    message(FATAL_ERROR "Falta firmware/main/secretos.h: copia secretos.h.example como secretos.h y llena tus datos")
endif()

idf_component_register(SRCS "main.c" "pantalla.c" "texto.c" "mic.c" "audio.c" "wifi.c"
                    PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_usb_serial_jtag
                                  esp_wifi esp_netif esp_event nvs_flash
                    INCLUDE_DIRS "")
