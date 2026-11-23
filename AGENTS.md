# Reglas del proyecto

Asistente de voz físico: ESP32-C3 → AgentCore (Strands + Nova). Diseño en `docs/arquitectura.md`, hardware en `docs/componentes.md`.

## Cómo trabajar

- El usuario quiere aprender: antes de cada paso explica qué haces y por qué, y presenta alternativas para que el usuario decida.
- Documentación corta. Idioma: español.

## Firmware (`firmware/`, ESP-IDF v5.4.2, target esp32c3)

Hay que activar el entorno **en la misma línea de shell** que el comando (el entorno no persiste entre shells):

```bash
eim select v5.4.2 && source /Users/rmatah/.espressif/tools/activate_idf_v5.4.2.sh && cd firmware && idf.py build
```

- Flashear y ver el log: `idf.py -p <puerto> flash monitor`
- Si `build/` da errores de ruta (el proyecto se movió de carpeta): `idf.py fullclean`
- Pruebas en la Mac (sin placa), desde `firmware/`:
  - Texto: `cc -fsanitize=address -I main test/test_texto.c main/texto.c -o /tmp/test_texto && /tmp/test_texto`
  - Audio: `cc -fsanitize=address -I main test/test_audio.c main/audio.c -lm -o /tmp/test_audio && /tmp/test_audio`

## Agente (`agent/`, Python 3.12 con uv)

- AWS: perfil `isengard`, región `us-east-1`. Si sale `LoginRefreshRequired`, el usuario debe correr `aws login --profile isengard`.
- Correr: `cd agent && AWS_PROFILE=isengard uv run agent.py` (texto) o `uv run voz.py` (micrófono).
- Prueba: `cd agent && uv run python test_agent.py`
