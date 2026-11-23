# Imagen del agente para AgentCore Runtime. AgentCore corre en Graviton: DEBE ser arm64.

# Etapa 1: dependencias exactas de uv.lock (sin las de desarrollo).
FROM --platform=linux/arm64 ghcr.io/astral-sh/uv:python3.12-bookworm-slim AS builder
WORKDIR /app
ENV UV_COMPILE_BYTECODE=1 UV_LINK_MODE=copy UV_PYTHON_DOWNLOADS=never
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev --no-install-project

# Etapa 2: solo el entorno y el código, sin uv ni cachés.
FROM --platform=linux/arm64 python:3.12-slim-bookworm
RUN useradd -r -u 1001 agente
WORKDIR /app
COPY --from=builder /app/.venv /app/.venv
COPY agent.py voz.py servidor.py ./
# DOCKER_CONTAINER: el SDK escucha en 0.0.0.0 solo dentro de un contenedor; aquí solo AgentCore llega.
ENV PATH="/app/.venv/bin:$PATH" DOCKER_CONTAINER=1 PYTHONUNBUFFERED=1
USER agente
EXPOSE 8080
CMD ["python", "servidor.py"]
