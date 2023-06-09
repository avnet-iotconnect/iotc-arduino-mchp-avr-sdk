## Introduction
IoTConnect C SDK for Microchip AVR Arduino boards like AVR IoT Cellular Mini

## Setup Instructions

* Follow the steps over the steps in the
 [Quick Start Guide for AVR® IoT Cellular Mini Board](https://www.hackster.io/grace-san-giacomo/quick-start-guide-for-avr-iot-cellular-mini-board-23e056)
 to connect the board components and register your Truphone SIM card.
* Follow the
 [AVR-IoT Cellular Mini UserGuide's Development Environment Section](https://iot.microchip.com/docs/arduino/introduction/devenv) 
to set up and prepare your Arduino development environment.
Ensure to follow the steps to install Arduino IDE version 2. 
The code in this repo has only been tested with Arduino IDE 2.0.4.
* Once your SIM is activated and Arduino environment set up, 
clone this repo into your Arduino sketch directory under the libraries directory.
For example, once you clone this SDK, the contents of this repo will be 
under *%USER%\Arduino\libraries\iotconnect-mchp-avr-sdk* in Windows.
* Open the Arduino IDE and load the avr-iot-provision example by navigating
to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-provision from the menu.
* Run the sketch by selecting the *Sketch->Upload Using Programmer* from the menu.
* You should see an output similar to this:

```
[INFO] HTTPS CA certificate updated successfuly.
[INFO] MQTT CA certificate updated successfuly.
[INFO] Initialized ECC608
[INFO] Ciphersuites config written successfully.
Device ID is avr-64c9fe1642154f1e44e9.
-----BEGIN CERTIFICATE-----
MIIB8DCCAZegAwIBAgIQZqxXFSHF/eOiwnn/0rSQLzAKBggqhkjOPQQDAjBPMSEw
HwYDVQQKDBhNaWNyb2NoaXAgVGVjaG5vbG9neSBJbmMxKjAoBgNVBAMMIUNyeXB0
byBBdXRoZW50aWNhdGlvbiBTaWduZXIgMkQzMDAgFw0yMTAzMjUxMTAwMDBaGA8y
MDQ5MDMyNTExMDAwMFowQjEhMB8GA1UECgwYTWljcm9jaGlwIFRlY2hub2xvZ3kg
SW5jMR0wGwYDVQQDDBRzbjAxMjNFRTdBMTQzMjlEM0QwMTBZMBMGByqGSM49AgEG
CCqGSM49AwEHA0IABNHmLcX7BUciDWCRoXyWM1UBd1/UeQWE93uvUa3Z3XHuoZis
naG+sYdmoGhgkfhwjYKH7eATjrSKeFPfX9c/vlOjYDBeMAwGA1UdEwEB/wQCMAAw
DgYDVR0PAQH/BAQDAgOIMB0GA1UdDgQWBBReZ3gqfZtIp+p4ZMn+FkIVTx5E6TAf
BgNVHSMEGDAWgBQss+/LXwRk0qR/1plYzq+aUB2NqTAKBggqhkjOPQQDAgNHADBE
AiByL9Qrcr9VC94fKPws5bIFd8a9YKFzp4ZPVuUJML863QIgFmCDPBO9zxRiJdLw
2qgjeuEeDVW6r0SVw4wpJSELhOY=
-----END CERTIFICATE-----
[INFO] ========= Provisioning Complete =========
```

* Obtain the fingerprint of the device certificate that is displayed on the screen.
This can be done with openssl command line or [this web site](https://www.samltool.com/fingerprint.php)
* Note the **fingerprint** and the **Device ID** value and use it in the next steps.

## IoTConnect Setup

* Log into your IoTConnect account and create a new template using the IoTConnect Web user interface.
use the Self-Signed authentication type and add one property called "version" of type STRING.
* Create a new device with name displayed on the provisioning sketch output above:
* Select your template created in the previous step.
* Paste your SHA1 or SHA256 fingeprint obtained in the previous steps as the *Primary Thumbprint* value. 
The pasted value should not contain colons. You may leave the *Secondary Thumbprint* value blank.

## Running the Demo

* Open the Arduino IDE and load the avr-iot-sample example by navigating
to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-sample from the menu.
* Edit the avr-iot-sample.cpp in the sketch par your account settings. Leave the IOTCONNECT_DUID blank:

```C
#define IOTCONNECT_CPID "your-cpid"
#define IOTCONNECT_ENV "your-env"
#define IOTCONNECT_DUID ""
```
* Run the sketch by selecting the *Sketch->Upload Using Programmer* from the menu.
* Upon successful connection, you should see an output similar to this:

```
INFO] Starting the Sample Application 01.00.00
[INFO] Connecting to operator....... OK!
[INFO] Obtained Device ID from the secure element: avr-64c9fe1642154f1e44e9
[INFO] +CCLK: "23/5/15,18:16:36-20"
[INFO] CPID: avtd***
[INFO] ENV:  Avnet
[INFO] Connecting to poc-iotconnect-iothub-030-eu2.azure-devices.net ....[INFO] 
IoTConnect Client Connected
[INFO] Sending: {"cpid":"avtds","dtg":"620c9902-17dd-46b0-a8a2-b20f77eed493","mt
":0,"sdk":{"l":"M_C","v":"2.0","e":"Avnet"},"d":[{"id":"avr-64c9fe1642154f1e44e9
","tg":"","d":[{"version":"01.00.00","cpu":3.1229999}],"dt":"2023-05-16T17:16:40
.000Z"}],"t":"2023-05-16T17:16:40.000Z"}
```
