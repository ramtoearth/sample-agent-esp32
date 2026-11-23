# /// script
# dependencies = ["pyserial"]
# ///
"""Monitor serie del ESP32 que además recibe grabaciones (botón BOOT), las guarda como WAV y las reproduce.

Uso: uv run firmware/tools/grabar.py [puerto]   (por defecto el primer /dev/cu.usbmodem*)
Protocolo del firmware: "<<AUDIO", líneas "A:<pcm en hex>", ">>AUDIO". El resto son logs.
"""

import glob
import subprocess
import sys
import wave

import serial

ARCHIVO = "grabacion.wav"
SAMPLE_RATE = 16000

puerto = sys.argv[1] if len(sys.argv) > 1 else sorted(glob.glob("/dev/cu.usbmodem*"))[0]
print(f"Escuchando {puerto} (Ctrl+C para salir). Mantén BOOT presionado y habla.")

with serial.Serial(puerto, timeout=1) as esp:
    trozos = None  # None = no estamos recibiendo una grabación
    while True:
        linea = esp.readline().decode(errors="replace").rstrip()
        if linea == "<<AUDIO":
            trozos = []
        elif linea.startswith("A:") and trozos is not None:
            trozos.append(bytes.fromhex(linea[2:]))
        elif linea == ">>AUDIO" and trozos is not None:
            pcm = b"".join(trozos)
            trozos = None
            with wave.open(ARCHIVO, "wb") as w:  # WAV = encabezado de 44 bytes + el mismo PCM
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(SAMPLE_RATE)
                w.writeframes(pcm)
            print(f"\n💾 {len(pcm) / 2 / SAMPLE_RATE:.1f} s guardados en {ARCHIVO} — reproduciendo...")
            subprocess.run(["afplay", ARCHIVO])
        elif linea:
            print(linea)
