"""Infraestructura del asistente: identidad del dispositivo y sus credenciales temporales.

ESP32 ──mTLS con su certificado──► IoT Core credentials provider ──► credenciales de 1 h del RolDispositivo
"""

from pathlib import Path

import aws_cdk as cdk
from aws_cdk import aws_iam as iam
from aws_cdk import aws_iot as iot
from constructs import Construct

THING = "asistente-c3"
ROLE_ALIAS = "asistente-dispositivo"
# La llave privada se generó en la Mac y nunca sale de ahí; a AWS solo va la CSR (llave pública).
CSR = Path(__file__).parent.parent / "firmware" / "certs" / "dispositivo.csr"


class AsistenteStack(cdk.Stack):
    def __init__(self, scope: Construct, id: str, **kwargs) -> None:
        super().__init__(scope, id, **kwargs)

        # Lo que puede hacer el dispositivo con sus credenciales temporales.
        # Vacío a propósito: en 4d se agrega solo el permiso para invocar el agente en AgentCore.
        rol = iam.Role(
            self,
            "RolDispositivo",
            assumed_by=iam.ServicePrincipal("credentials.iot.amazonaws.com"),  # solo IoT Core puede asumirlo
            max_session_duration=cdk.Duration.hours(1),
            description="Credenciales temporales del ESP32 del asistente (vía IoT Core)",
        )

        # El nombre que usa el dispositivo en la URL para pedir credenciales de `rol`.
        alias = iot.CfnRoleAlias(
            self, "RoleAlias", role_alias=ROLE_ALIAS, role_arn=rol.role_arn, credential_duration_seconds=3600
        )

        cert = iot.CfnCertificate(self, "Certificado", status="ACTIVE", certificate_signing_request=CSR.read_text())
        thing = iot.CfnThing(self, "Thing", thing_name=THING)

        # Permiso del certificado: SOLO pedir credenciales de este role alias (ni MQTT ni nada más).
        politica = iot.CfnPolicy(
            self,
            "Politica",
            policy_document={
                "Version": "2012-10-17",
                "Statement": [
                    {"Effect": "Allow", "Action": "iot:AssumeRoleWithCertificate", "Resource": alias.attr_role_alias_arn}
                ],
            },
        )
        iot.CfnPolicyPrincipalAttachment(self, "PoliticaCert", policy_name=politica.ref, principal=cert.attr_arn)
        iot.CfnThingPrincipalAttachment(self, "ThingCert", thing_name=thing.ref, principal=cert.attr_arn)

        cdk.CfnOutput(self, "CertificadoId", value=cert.ref)
        cdk.CfnOutput(self, "RolArn", value=rol.role_arn)


app = cdk.App()
AsistenteStack(app, "Asistente", env=cdk.Environment(region="us-east-1"))
cdk.Tags.of(app).add("Proyecto", "asistente")
app.synth()
