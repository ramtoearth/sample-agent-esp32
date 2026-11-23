# Como el build.rs del template: fallar pronto y con instrucciones si faltan los secretos o los certificados.
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/secretos.h")
    message(FATAL_ERROR "Falta firmware/main/secretos.h: copia secretos.h.example como secretos.h y llena tus datos")
endif()
set(CERTS "../certs/AmazonRootCA1.pem" "../certs/dispositivo.crt" "../certs/dispositivo.key" "../certs/endpoint.txt" "../certs/agente_arn.txt")
foreach(f ${CERTS})
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${f}")
        message(FATAL_ERROR "Falta firmware/certs/${f}: despliega infra/ y corre `AWS_PROFILE=isengard infra/certs.sh`")
    endif()
endforeach()

idf_component_register(SRCS "main.c" "pantalla.c" "texto.c" "mic.c" "audio.c" "wifi.c" "hora.c" "credenciales.c" "sigv4.c" "sigv4_mbedtls.c" "agente.c"
                    PRIV_REQUIRES esp_lcd esp_driver_i2c esp_driver_i2s esp_driver_gpio
                                  esp_wifi esp_netif esp_event nvs_flash esp_http_client json mbedtls
                    INCLUDE_DIRS ""
                    EMBED_TXTFILES ${CERTS}) # se incrustan en el binario terminados en '\0'
