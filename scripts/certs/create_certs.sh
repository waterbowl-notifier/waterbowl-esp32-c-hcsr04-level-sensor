#!/bin/bash

# Function to create a certificate and attach it to a thing
create_and_attach_cert() {
  local thing=$1

  # Create certificate and capture the response
  response=$(aws iot create-keys-and-certificate --profile tennis@gamename --region us-east-1 --set-as-active --output json)
  
  # Check if the response is valid JSON
  if ! echo "$response" | jq empty; then
    echo "Failed to create certificate for $thing"
    return
  fi

  # Extract certificate ARN and save certificate files
  certificateArn=$(echo $response | jq -r '.certificateArn')
  certificatePem=$(echo $response | jq -r '.certificatePem')
  publicKey=$(echo $response | jq -r '.keyPair.PublicKey')
  privateKey=$(echo $response | jq -r '.keyPair.PrivateKey')
  certificateId=$(echo $response | jq -r '.certificateId')

  # Save certificates to files
  echo "$certificatePem" > "$thing-certificate.pem"
  echo "$publicKey" > "$thing-public.pem.key"
  echo "$privateKey" > "$thing-private.pem.key"

  # Attach certificate to the Thing
  aws iot attach-thing-principal --thing-name "$thing" --principal "$certificateArn" --profile tennis@gamename --region us-east-1 

#  aws iot attach-policy --policy-name WaterBowlIoTPolicy --target "$certificateArn" --profile tennis@gamename --region us-east-1 

  echo "Generated and attached certificate for $thing"
}

# Create the Thing Type
aws iot create-thing-type --profile tennis@gamename --region us-east-1  --thing-type-name "water-bowl" 2>/dev/null || echo "Thing Type 'water-bowl' already exists."

# List of things to create certificates for
things=("living-room" "dining-room")

# Loop through each thing and create certificates
for thing in "${things[@]}"; do
  create_and_attach_cert "$thing"
done
