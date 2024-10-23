#!/bin/bash

# Replace this with your bucket name
BUCKET_NAME="charlies-farm-ota"

# Fetch the list of files with their last modified dates
aws s3api list-objects-v2 \
    --profile tennis@charliesfarm \
    --region us-east-2 \
    --bucket "$BUCKET_NAME" \
    --query 'Contents[*].[Key, LastModified]' \
    --output text |
# Sort by the second column (LastModified) in ascending order
sort -k2 |
# Format the output to display file name and date
awk '{print $1, $2}'
