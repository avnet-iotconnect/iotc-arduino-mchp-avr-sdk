## Introduction
This QuickStart Guide will enable rapid evaluation of the Avnet IoTConnect platform using the C SDK on the Microchip AVR IoT Cellular Mini Kit.  This can also be used for other similar AVR Arduino boards.

For developing software leveraging the IoTConnect C SDK refer to the [Developer Guide](README.md).

## SIM Card Activation

* The AVR-IoT Cell Mini Kit contains a Truphone SIM. Follow the steps below to create an account and activate it.
  * Create an account or log in with an existing account at the [Truphone Sign-up page](https://account.truphone.com/register).
  * Activate the SIM from the account dashboard page by following the steps outlined in the activation process. 
    See the screenshot below for reference:

![Truphone Activation](media/truphone-activate-sim.png "Truphone Activation")

* Install the SIM into board by sliding the notched-corner end of the SIM in with the Truphone logo facing outwards.

## Cloud Account Setup
An IoTConnect account is required to continue this guide. If you need to create an account, a free 2-month subscription is available.  Please follow the [Creating a New IoTConnect Account](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/main/documentation/iotconnect/subscription/subscription.md) guide and return to this guide once complete.
* The IoTConnect **CPID** and **Environment** values will be required when provisioning the board. These values can be located in the IoTConnect WebUI on the Key Vault page. Copy these values for future use.

## IoTConnect Firmware
***Note:** This process will require a USB A to USB C cable which is **not included** in the AVR-IoT Cell Mini kit.

* Download and extract the pre-compiled firmware ([avr-iot-03.00.00.zip](https://saleshosted.z13.web.core.windows.net/sdk/arduino/avr-iot-03.00.00.zip)).
* The package contains two files:
  * **avr-iot-provision.ino.hex:** Provisioning firmware. This will need to be loaded and run once to provision the board with the IoTConnect CPID and Environment information.
  * **avr-iot-sample.ino.hex:** Sample firmware. This is the actual sample code which should be loaded only after the board has been provisioned.
* Connect the board to your PC using a USB A to USB C cable.
* A new virtual drive will appear in your Windows Explorer.
* Follow the provisioning steps in the section below.

## 7. Cloud Account Setup
An IoTConnect account is required.  If you need to create an account, a free trial subscription is available.

Select one of the two implementations of IoTConnect:
* [AWS Version](https://subscription.iotconnect.io/subscribe?cloud=aws)  (Recommended)
* [Azure Version](https://subscription.iotconnect.io/subscribe?cloud=azure)  

> [!NOTE]
> Be sure to check any SPAM folder for the temporary password after registering.

## Acquire IoTConnect Account Information
Login to IoTConnect using the corresponding link below to the version for which you registered:  
* [IoTConnect on AWS](https://console.iotconnect.io) (Recommended)
* [IoTConnect on Azure](https://portal.iotconnect.io)

The Company ID (**CPID**) and Environment (**ENV**) variables identifying your IoTConnect account must be configured for the device.
* Take note of these values for later reference located in the "Settings" -> "Key Vault" section of the platform. See image below.

<img src="https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/blob/bbdc9f363831ba607f40805244cbdfd08c887e78/assets/cpid_and_env.png" width=600>

## Provisioning

* Install a serial console application, such as [Tera Term](https://ttssh2.osdn.jp/index.html.en).
* Open the serial console application and establish a connection to the board in order to see the provisioning output.
* Load the **Provisioning Firmware** (avr-iot-provision.ino.hex) onto the board by copying it into the root of the Virtual Drive that was created in the previous section.
* After the copy is complete, remove power from the board and reapply after 5 seconds.
* Once the board boots, verify that there is output in the serial console that looks similar to the example below.

```
[INFO] Starting the provisioning sample...
-----BEGIN CERTIFICATE-----
MIIB8DCCAZegAwIBAgIQZqxXFSHF/eOiwnn/0rSQLzAKBggqhkjOPQQDAjBPMSEw
HwYDVQQKDBhNaWNyb2NoaXAgVGVjaG5vbG9neSBJbmMxKjAoBgNVBAMMIUNyeXB0
byBBdXRoZW50aWNhdGlvbiBTaWduZXIgMkQzMDAgFw0yMTAzMjUxMTAwMDBaGA8y
MDQ5MDMyNTExMDA------------------------jcm9jaGlwIFRlY2hub2xvZ3kg
SW5jMR0wGwYDVQQ    THIS IS A SAMPLE    iOkQwMTBZMBMGByqGSM49AgEG
CCqGSM49AwEHA0I------------------------Bd1/UeQWE93uvUa3Z3XHuoZis
naG+sYdmoGhgkfhwjYKH7eATjrSKeFPfX9c/vlOjYDBeMAwGA1UdEwEB/wQCMAAw
DgYDVR0PAQH/BAQDAgOIMB0GA1UdDgQWBBReZ3gqfZtIp+p4ZMn+FkIVTx5E6TAf
BgNVHSMEGDAWgBQss+/LXwRk0qR/1plYzq+aUB2NqTAKBggqhkjOPQQDAgNHADBE
AiByL9Qrcr9VC94fKPws5bIFd8a9YKFzp4ZPVuUJML863QIgFmCDPBO9zxRiJdLw
2qgjeuEeDVW6r0SVw4wpJSELhOY=
-----END CERTIFICATE-----

Current provisioning data:
...
```
* **Copy** the Device Unique ID *(DUID)* from the terminal and save for later use.
* Use the console at this point to provision your code with information found in your account:
  * Platfrom: For IoTConnect on AWS enter ```aws```, or for Azure enter ```az``` 
  * CPID: Value noted in the previous step [Acquire IoTConnect Account Information](#acquire-iotconnect-account-information)
  * CPID: Value noted in the previous step [Acquire IoTConnect Account Information](#acquire-iotconnect-account-information)
* You may choose to enter a blank Device Unique ID (DUID), and in that case the default 
auto-generated ID, unique for each board will used. This ID will be printed on the screen
* Note or save the device certificate displayed on the terminal (including the BEGIN and END lines) 
 and the **Device ID** value, and use it in the next steps

## IoTConnect Device Template Setup

An IoTConnect *Device Template* will need to be created or imported. This defines the data format the platform should expect from the device.
* Download the premade  [Device Template](files/avriot-template.json?raw=1) (**must** Right-Click the link, Save As)
 
* **Click** the Device icon and the "Device" sub-menu:  
<img src="https://github.com/avnet-iotconnect/avnet-iotc-mtb-xensiv-example/assets/40640041/57e0b0c8-08ba-4c3f-b33d-489d7d0db568" width=200>

* At the bottom of the page, select the "Templates" icon from the toolbar.<br>![image](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/assets/40640041/3dc0b82c-13ea-4d99-93be-3adf14575709)
* At the top-right of the page, select the "Create Template" button.<br>![image](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/assets/40640041/33325cbd-4fee-4958-b32a-f28d0d52342c)
* At the top-right of the page, select the "Import" button.<br>![image](https://github.com/avnet-iotconnect/avnet-iotconnect.github.io/assets/40640041/418b999c-58e2-49f3-a3f1-118b16271b26)
* Finally, click the "Browse" button and select the template previously downloaded.

## IoTConnect Device Creation
* **Click** the Device icon and the "Device" sub-menu:  
<img src="https://github.com/avnet-iotconnect/avnet-iotc-mtb-xensiv-example/assets/40640041/57e0b0c8-08ba-4c3f-b33d-489d7d0db568" width=200>

* At the top-right, click on the "Create Device" button:  
<img src="https://github.com/avnet-iotconnect/avnet-iotc-mtb-xensiv-example/assets/40640041/82e70cb6-018b-4bf3-a92c-a7286b05d73f" width=200>


* Enter the **DUID** saved from earlier in the *Unique ID* field
* Enter a description of your choice in the *Display Name* to help identify your device
* Select the template from the dropdown box that was just imported ("avriot")
* Ensure "Use my certificate" is selected under *Device certificate*
* Browse and Select the "cert.txt" file saved in the previous [Provisioning](#provisioning) step or simply paste the certificate
* Click **Save & View**

## Running Demo Code with Arduino IDE

* Load the Sample firmware image avr-iot-sample.ino.hex. 
* On a successful run, you should see an output similar to this:

```
[INFO] Starting the Sample Application 03.00.00
[INFO] CPID: [your CPID]
[INFO] Env : [your Env]
[INFO] DUID: avr-092ee282bb58cf55f34c66e3d3c
[INFO] Connecting to operator......... OK!
[INFO] +CCLK: "23/6/26,13:22:7-20"
[INFO] CPID: [your CPID]
[INFO] ENV:  [your Env]
[INFO] Attempting to connect to MQTT host:poc-iotconnect-iothub-030-eu2.azure-de
vices.net, client id:avtds-avr-092ee282bb58cf55f34c66e3d3c, username: poc-iotcon
nect-iothub-030-eu2.azure-devices.net/avtds-avr-092ee282bb58cf55f34c66e3d3c/?api
-version=2018-06-30
[INFO] IoTConnect Client Connected
[INFO] Sending: {"cpid":"****","dtg":"620c9902-17dd-46b0-a8a2-b20f77eed493","mt
":0,"sdk":{"l":"M_C","v":"2.0","e":"***"},"d":[{"id":"avr-092ee282bb58cf55f34c
66e3d3c","tg":"","d":[{"version":"01.00.00","cpu":3.1229999}],"dt":"2023-05-16T1
7:16:40.000Z"}],"t":"2023-05-16T17:16:40.000Z"}
```
