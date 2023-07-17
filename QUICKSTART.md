## Introduction
This is a QuickStart Guide for IoTConnect C SDK for Microchip AVR Arduino boards like AVR IoT Cellular Mini

This guide is for those interested in evaluating and testing this board with IoTConnect.

If you are interested in developing software with our IoTConnect C SDK, 
then you should refer to the [Developer Guide](README.md)

## SIM Card Activation

* Your board package should contain a Truphone SIM. Follow these steps to activate it:
  * Create an account or log in at the [Truphone Sign up page](https://account.truphone.com/register)
  * Activate your SIM from your account's dashboard page and following the steps outlined in the activation process. 
    See the screenshot below:

![Truphone Activation](media/truphone-activate-sim.png "Truphone Activation")

* Install the sim into the SIM slot on the board.

## IoTConnect Firmware Loading

* Download and extract the firmware at [this URL](https://saleshosted.z13.web.core.windows.net/sdk/arduino/avr-iot-dev-062623.zip)
* The package will contain two files:
  * avr-iot-provision.ino.hex: Provisioning firmware. You need to run this firmware once, so that it can configure the board.
  * avr-iot-sample.ino.hex: Sample firmware. You need to run this firmware once, so that it can configure the board.
* Connect the board to your PC using the Micro USB cable.
* A new virtual drive will appear in your Windows Explorer.
* You can drag and drop or copy-paste the appropriate file into the new drive in order to load the firmware for each of the two sections below.

## Provisioning

* Install a serial console application, such as [Tera Term](https://ttssh2.osdn.jp/index.html.en).
* Open the serial console application and establish a connection to the board in order to see the provisioning output.
* Load the Provisioning firmware image avr-iot-provision.ino.hex.
* Once the board boots, you should see an output similar to this in the serial console:

```
[INFO] Starting the provisioning sample...
[INFO] Ciphersuites config written successfully.
[INFO] HTTPS CA certificate updated successfuly.
[INFO] MQTT CA certificate updated successfuly.
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
...
```
* Use the console at this point to provision your code with information found in your account:
  * CPID
  * Environment
* You may choose to enter a blank Device Unique ID (DUID), and in that case the default 
auto-generated ID, unique for each board will user. This ID will be printed on the screen.
* Obtain the fingerprint of the device certificate that is displayed on the screen above.
This can be done with openssl command line or [this web site](https://www.samltool.com/fingerprint.php)
* Note the **fingerprint** and the **Device ID** value and use it in the next steps.

## Running Demo Code with Arduino IDE

* Load the Sample firmware image avr-iot-sample.ino.hex. 
* On a successful run, you should see an output similar to this:

```
[INFO] Starting the Sample Application 01.00.00
[INFO] CPID: avtds
[INFO] Env : Avnet
[INFO] DUID: avr-092ee282bb58cf55f34c66e3d3c
[INFO] Connecting to operator......... OK!
[INFO] +CCLK: "23/6/26,13:22:7-20"
[INFO] CPID: avtd***
[INFO] ENV:  Avnet
[INFO] Attempting to connect to MQTT host:poc-iotconnect-iothub-030-eu2.azure-de
vices.net, client id:avtds-avr-092ee282bb58cf55f34c66e3d3c, username: poc-iotcon
nect-iothub-030-eu2.azure-devices.net/avtds-avr-092ee282bb58cf55f34c66e3d3c/?api
-version=2018-06-30
[INFO] IoTConnect Client Connected
[INFO] Sending: {"cpid":"avtds","dtg":"620c9902-17dd-46b0-a8a2-b20f77eed493","mt
":0,"sdk":{"l":"M_C","v":"2.0","e":"Avnet"},"d":[{"id":"avr-092ee282bb58cf55f34c
66e3d3c","tg":"","d":[{"version":"01.00.00","cpu":3.1229999}],"dt":"2023-05-16T1
7:16:40.000Z"}],"t":"2023-05-16T17:16:40.000Z"}
```