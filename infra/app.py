"""Infraestructura del asistente (fuente única de los recursos de AWS del proyecto).

ESP32 ──mTLS con su certificado──► IoT Core credentials provider ──► credenciales de 1 h del RolDispositivo
ESP32 ──WebSocket firmado (SigV4)──► AgentCore Runtime "asistente" (agent/: Transcribe + Strands)
"""

from pathlib import Path

import aws_cdk as cdk
from aws_cdk import aws_bedrockagentcore as agentcore
from aws_cdk import aws_ecr_assets as ecr_assets
from aws_cdk import aws_iam as iam
from aws_cdk import aws_iot as iot
from aws_cdk import aws_logs as logs
from constructs import Construct

THING = "asistente-c3"
ROLE_ALIAS = "asistente-dispositivo"
# La llave privada se generó en la Mac y nunca sale de ahí; a AWS solo va la CSR (llave pública).
CSR = Path(__file__).parent.parent / "firmware" / "certs" / "dispositivo.csr"
AGENTE = Path(__file__).parent.parent / "agent"
MODELO = "amazon.nova-micro-v1:0"  # = BedrockModel en agent/agent.py


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

        # ── Agente en AgentCore Runtime ──────────────────────────────────────────────────────────────
        # La imagen se construye desde agent/ (Dockerfile arm64) y se sube al ECR del bootstrap.
        # Con Finch: CDK_DOCKER=finch cdk deploy
        imagen = ecr_assets.DockerImageAsset(
            self, "ImagenAgente", directory=str(AGENTE), platform=ecr_assets.Platform.LINUX_ARM64
        )

        # Lo que puede hacer el contenedor. Solo AgentCore de ESTA cuenta puede asumirlo (confused deputy).
        rol_agente = iam.Role(
            self,
            "RolAgente",
            assumed_by=iam.ServicePrincipal(
                "bedrock-agentcore.amazonaws.com",
                conditions={
                    "StringEquals": {"aws:SourceAccount": self.account},
                    "ArnLike": {"aws:SourceArn": f"arn:aws:bedrock-agentcore:{self.region}:{self.account}:*"},
                },
            ),
            description="Ejecución del agente del asistente en AgentCore",
        )
        imagen.repository.grant_pull(rol_agente)  # descargar la imagen
        rol_agente.add_to_policy(
            iam.PolicyStatement(  # Strands usa el modo streaming
                actions=["bedrock:InvokeModel", "bedrock:InvokeModelWithResponseStream"],
                resources=[f"arn:aws:bedrock:{self.region}::foundation-model/{MODELO}"],
            )
        )
        rol_agente.add_to_policy(
            iam.PolicyStatement(  # Transcribe no admite permisos por recurso: "*" es lo más acotado posible
                actions=["transcribe:StartStreamTranscription"], resources=["*"]
            )
        )
        grupo_logs = f"arn:aws:logs:{self.region}:{self.account}:log-group:/aws/bedrock-agentcore/runtimes/*"
        rol_agente.add_to_policy(
            iam.PolicyStatement(
                actions=["logs:CreateLogGroup", "logs:CreateLogStream", "logs:PutLogEvents", "logs:DescribeLogStreams"],
                resources=[grupo_logs],
            )
        )
        rol_agente.add_to_policy(
            iam.PolicyStatement(
                actions=["logs:DescribeLogGroups"],
                resources=[f"arn:aws:logs:{self.region}:{self.account}:log-group:*"],
            )
        )

        runtime = agentcore.CfnRuntime(
            self,
            "Agente",
            agent_runtime_name="asistente",
            agent_runtime_artifact=agentcore.CfnRuntime.AgentRuntimeArtifactProperty(
                container_configuration=agentcore.CfnRuntime.ContainerConfigurationProperty(
                    container_uri=imagen.image_uri
                )
            ),
            role_arn=rol_agente.role_arn,
            # PUBLIC = el contenedor sale directo a Bedrock/Transcribe. La invocación exige SigV4 igual.
            network_configuration=agentcore.CfnRuntime.NetworkConfigurationProperty(network_mode="PUBLIC"),
            protocol_configuration="HTTP",  # /ping, /invocations y el WebSocket /ws en el puerto 8080
            description="Asistente de voz: Transcribe + Strands (Nova Micro)",
        )
        runtime.node.add_dependency(rol_agente)  # los permisos deben existir antes de arrancar el contenedor

        # Logs: se borran solos a los 7 días (servidor.py no registra el contenido). AgentCore crea el
        # grupo al crear el runtime, así que no se puede crear antes: LogRetention ajusta el existente.
        logs.LogRetention(
            self,
            "RetencionLogs",
            log_group_name=f"/aws/bedrock-agentcore/runtimes/{runtime.attr_agent_runtime_id}-DEFAULT",
            retention=logs.RetentionDays.ONE_WEEK,
        )

        cdk.CfnOutput(self, "AgenteArn", value=runtime.attr_agent_runtime_arn)
        cdk.CfnOutput(self, "CertificadoId", value=cert.ref)
        cdk.CfnOutput(self, "RolArn", value=rol.role_arn)


app = cdk.App()
AsistenteStack(app, "Asistente", env=cdk.Environment(region="us-east-1"))
cdk.Tags.of(app).add("Proyecto", "asistente")
app.synth()
