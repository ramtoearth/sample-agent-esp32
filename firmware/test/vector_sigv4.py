"""Genera la URL prefirmada de referencia para test_sigv4.c con el SDK oficial (botocore) y hora fija.
Credenciales FALSAS (no sirven para nada). Uso: cd agent && uv run python ../firmware/test/vector_sigv4.py
"""
import datetime
from unittest import mock

import boto3
from bedrock_agentcore.runtime import AgentCoreRuntimeClient

sesion = boto3.Session(aws_access_key_id="ASIAEJEMPLO0000000000", aws_secret_access_key="secretoFalso/0123456789+abcdefghijklmnopqr",
                       aws_session_token="TokenFalso//con+simbolos=yMas==", region_name="us-east-1")
fijo = datetime.datetime(2026, 11, 23, 3, 4, 5, tzinfo=datetime.timezone.utc)
with mock.patch("botocore.auth.get_current_datetime", return_value=fijo):
    print(AgentCoreRuntimeClient(region="us-east-1", session=sesion).generate_presigned_url(
        runtime_arn="arn:aws:bedrock-agentcore:us-east-1:123456789012:runtime/remi-AbCdE12345",
        session_id="sesion-de-prueba-0123456789abcdef0123", expires=300))
