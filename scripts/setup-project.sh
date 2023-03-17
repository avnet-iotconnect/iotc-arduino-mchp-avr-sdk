#!/bin/bash

set -e # fail on errors

cd $(dirname "$0")/../

git archive HEAD --prefix=iotc-mchp-avr-sdk/  --format=zip -o iotc-mchp-avr-sdk.zip
