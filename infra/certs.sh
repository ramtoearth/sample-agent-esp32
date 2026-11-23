#!/bin/sh
# Descarga lo que necesita el dispositivo después de `cdk deploy` (a firmware/certs/, ignorado por git).
# Uso: AWS_PROFILE=isengard ./certs.sh
set -eu
D="$(dirname "$0")/../firmware/certs"
CERT_ID=$(aws cloudformation describe-stacks --stack-name Asistente --region us-east-1 \
  --query "Stacks[0].Outputs[?OutputKey=='CertificadoId'].OutputValue" --output text)
aws iot describe-certificate --certificate-id "$CERT_ID" --region us-east-1 \
  --query certificateDescription.certificatePem --output text > "$D/dispositivo.crt"
curl -sf https://www.amazontrust.com/repository/AmazonRootCA1.pem -o "$D/AmazonRootCA1.pem"
echo "Certificados en $D"
echo "Endpoint de credenciales: $(aws iot describe-endpoint --endpoint-type iot:CredentialProvider --region us-east-1 --query endpointAddress --output text)"
