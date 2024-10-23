#!/bin/bash

aws iot create-thing-type \
    --profile tennis@gamename \
    --region us-east-1 \
    --thing-type-name "water-bowl" 

# Read the JSON file
things=$(jq -c '.things[]' things.json)

# Loop through each thing and create it
for thing in $things; do
  thingName=$(echo $thing | jq -r '.thingName')
  thingTypeName=$(echo $thing | jq -r '.thingTypeName')
  attributes=$(echo $thing | jq -r '.attributePayload.attributes | tojson')
  
  aws iot create-thing \
    --profile tennis@gamename \
    --region us-east-1 \
    --thing-name $thingName \
    --thing-type-name $thingTypeName \
    --attribute-payload "{\"attributes\":$attributes}" 
  
  echo "Created thing: $thingName"
done
