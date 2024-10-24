#!/bin/bash

PROFILE="tennis@gamename"
REGION="us-east-1"

DINING_MAC="dc:da:0c:c1:bb:14"
LIVING_MAC="30:30:f9:fb:8c:8c"

BUCKET="gamename-iot-ota"

IOT_URL="https://a3u37c52vq0b6j-ats.iot.us-east-1.amazonaws.com"
IOT_TOPIC="local/waterbowl/update"

get_latest_firmware_dir() {
  BUCKET_NAME=$1

  # List all the firmware.bin files in the bucket, sorted by the last modified date, and get the newest one.
  LATEST_FIRMWARE_PATH=$(aws s3api list-objects-v2 \
      --profile "${PROFILE}" \
      --region "${REGION}" \
      --bucket "$BUCKET_NAME" \
      --query "Contents[?contains(Key, 'firmware.bin')].[Key, LastModified]" \
      --output text | sort -k2r | head -n1 | awk '{print $1}')

  if [[ -n "$LATEST_FIRMWARE_PATH" ]]; then
    # Remove the filename and extract the last directory in the path
    LAST_DIR=$(basename "$(dirname "$LATEST_FIRMWARE_PATH")")
    echo "$LAST_DIR"
  else
    echo "No firmware.bin found in the bucket."
  fi
}

# Example usage:
LATEST=$(get_latest_firmware_dir $BUCKET)
echo "Latest firmware directory: $LATEST"

aws iot-data publish \
    --profile "${PROFILE}" \
    --region "${REGION}" \
    --endpoint-url "${IOT_URL}" \
    --topic "${IOT_TOPIC}" \
    --cli-binary-format raw-in-base64-out \
    --payload \
      "{\"${DINING_MAC}\": \"https://${BUCKET}.s3.${REGION}.amazonaws.com/home/dining-room/water-bowl/${LATEST}/firmware.bin\",
        \"${LIVING_MAC}\": \"https://${BUCKET}.s3.${REGION}.amazonaws.com/home/living-room/water-bowl/${LATEST}/firmware.bin\"}"
