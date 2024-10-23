#!/bin/bash

CHARLIE_MAC="f0:f5:bd:8f:fa:e0"
TENNIS_MAC="10:91:a8:1f:cb:14"
FARM_MAC="dc:da:0c:c1:b7:ec"
TEST_MAC="f0:f5:bd:8f:f9:a8"

BUCKET="charlies-farm-ota"

PAYLOAD_MAC=${CHARLIE_MAC}
HOUSE="charlie-house"

get_latest_firmware_dir() {
  BUCKET_NAME=$1

  # List all the firmware.bin files in the bucket, sorted by the last modified date, and get the newest one.
  LATEST_FIRMWARE_PATH=$(aws s3api list-objects-v2 \
      --profile tennis@charliesfarm \
      --region us-east-2 \
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
    --profile tennis@charliesfarm \
    --region us-east-2 \
    --endpoint-url "https://ay1nsbhuqfhzk-ats.iot.us-east-2.amazonaws.com" \
    --topic "farm/coop/snooper/update" \
    --cli-binary-format raw-in-base64-out \
    --payload "{\"${PAYLOAD_MAC}\": \"https://${BUCKET}.s3.us-east-2.amazonaws.com/coop-snooper/${HOUSE}/${LATEST}/firmware.bin\"}"

# Uncomment and use the following payload if needed
# aws iot-data publish \
#     --profile tennis@charliesfarm \
#     --region us-east-2 \
#     --endpoint-url "https://ay1nsbhuqfhzk-ats.iot.us-east-2.amazonaws.com" \
#     --topic "farm/coop/snooper/update" \
#     --cli-binary-format raw-in-base64-out \
#     --payload "{\"10:91:a8:1f:cb:14\": \"https://${BUCKET}.s3.us-east-2.amazonaws.com/coop-snooper/tennis-house/${LATEST}/firmware.bin\",
#                 \"dc:da:0c:c1:b7:ec\": \"https://${BUCKET}.s3.us-east-2.amazonaws.com/coop-snooper/farm-house/${LATEST}/firmware.bin\",
#                 \"dc:da:0c:c1:bc:14\": \"https://${BUCKET}.s3.us-east-2.amazonaws.com/coop-snooper/test/${LATEST}/firmware.bin\"}"