#!/bin/bash

set -e # fail on errors

cd $(dirname "$0")

if [[ "$1" == "github-action-upload" ]]; then
  rm -rf /tmp/iotc-mchp-avr-sdk
  mkdir -p /tmp/iotc-mchp-avr-sdk
  cp -r * /tmp/iotc-mchp-avr-sdk
  rm -rf /tmp/iotc-mchp-avr-sdk/scripts
  mv /tmp/iotc-mchp-avr-sdk .

else
  cd ../
  git archive HEAD --prefix=iotc-mchp-avr-sdk/  --format=zip -o iotc-mchp-avr-sdk.zip
fi
