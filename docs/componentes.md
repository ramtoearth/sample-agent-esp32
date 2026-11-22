# Componentes

| Componente | Estado | Notas |
|---|---|---|
| ESP32-C3 Super Mini | ✅ Tengo | Un solo I2S → mic y ampli comparten BCLK/WS |
| OLED SSD1306 0.96" 128x64 I2C | ✅ Tengo | |
| Micrófono INMP441 (I2S) | ✅ Tengo | L/R a GND |
| Amplificador MAX98357A (I2S) | ⏳ Falta | |
| Bocina 8 Ω, 1–2 W | ⏳ Falta | No estaba en el diagrama |
| Botón push-to-talk | ⏳ Falta | Mientras tanto: botón BOOT (GPIO9) |
| Batería LiPo 3.7 V | ⏳ Falta | |
| Cargador TP4056 USB-C con protección | ⏳ Falta | |
| Interruptor | ⏳ Falta | |
| Capacitor 470–1000 µF | ⏳ Falta | Picos de corriente del WiFi |
| 2× resistencia 100 kΩ | ⏳ Falta | Divisor para medir la batería |

## Pines

| Señal | GPIO |
|---|---|
| I2S BCLK (mic SCK + ampli BCLK) | 4 |
| I2S WS (mic WS + ampli LRC) | 5 |
| Mic SD | 6 |
| Ampli DIN | 3 |
| OLED SDA / SCL | 21 / 20 |
| Botón | 9 (BOOT) → luego un pin libre |
| ADC batería | 0 |

GPIO2 y GPIO8 se evitan porque son pines de arranque.
