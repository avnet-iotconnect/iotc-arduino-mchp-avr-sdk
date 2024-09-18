## Introduction
IoTConnect C SDK for Microchip AVR Arduino board [AVR IoT Cellular Mini](https://www.microchip.com/en-us/development-tool/ev70n78a).

This guide is focusing on developing software with our IoTConnect C SDK.

If you are interested in evaluating and testing this board with IoTConnect,
then you may be interested in trying out our [Quickstart Guide](QUICKSTART.md)

## Prerequisites
This guide has been tested with the following environment:
* PC with Windows 10/11
* USB-A to USB-C data cable
* A serial terminal application such as [Tera Term](https://sourceforge.net/projects/tera-term/) (Recommended) or a browser-based version such as [Google Chrome Labs Serial Terminal](https://googlechromelabs.github.io/serial-terminal/)

## About the Application

The application will record the number of times that the user button is pressed on the board along 
with temperature and light sensor reading and report the following telemetry values:

| Value          | Description                                                         |
|----------------|---------------------------------------------------------------------|
| version        | $250                                                                |
| random         | $80                                                                 |
| temperature    | $420                                                                |
| light          | Light sensor readout (0 - 255), as object with the attributes below |
| light.r        | Red sensor                                                          |
| light.g        | Green sensor                                                        |
| light.b        | Blue sensor                                                         |
| light.ir       | Infrared sensor                                                     |
| button_counter | The  number of times the user button is pressed since board reboot  |

In the future implementation, the board will react to the following commands - see [Known Issues](#known-issues).
The following table is provided as reference:

| Command   | Arguments | Description                   |
|-----------|-----------|-------------------------------|
| user-led  | on/off    | Turns the User LED on or off  | 
| error-lef | on/off    | Turns the Error LED on or off |

## Development Setup Instructions

* Follow the [Quickstart Guide](QUICKSTART.md) to set up the board, Truphone account and sim activation and install the sim into the board.
* Arduino IDE in Carriage Return input mode may suffice as a terminal, but it is recommended to install a serial console application, 
such as [Tera Term](https://ttssh2.osdn.jp/index.html.en).

* Follow the
 [AVR-IoT Cellular Mini UserGuide's Development Environment Section](https://iot.microchip.com/docs/arduino/introduction/devenv) 
to set up and prepare your Arduino development environment.
The project has been tested with Arduino IDE 2.3.2, AVR-IoT-Cellular library version 1.3.11 (minimum required) and DXCore version 1.5.11.
It is recommended that you install these specific versions.
* Once your SIM is activated and Arduino environment set up, 
clone this repo into your Arduino sketch directory under the *libraries* directory.
For example, once you clone this SDK, the contents of this repo should be 
under *C:\Users\\<your username>\Documents\Arduino\libraries\iotconnect-mchp-avr-sdk* in Windows

# (Optional) Patching the AVR-IoT-Cellular-Mini Library Source

While the original library source will work, there are a few changes that are applied to the binary build and should also
be applied to our Arduino environment. The patches add stability fixes and additional logging to the library when log
level is set to LogLevel::Debug in the Sketch source files.

Apply the patched files under the [reference-files/updated] directory to the 1.3.11 version of the 
AVR-IoT-Cellular-Mini Library Source, which should be located under 
*C:\Users\\<your username>\Documents\Arduino\libraries\AVR-IoT-Cellular\src* in Windows. If using a newer version
of the library, you can diff the changes between [reference-files/updated] and [reference-files/original] to devise 
a set of patches that would need to be applied.

## Development Build

* Refer to to the [Quickstart Guide](QUICKSTART.md) for the next steps, 
 where instead of using pre-compiled binaries, we will be build the corresponding firmware using the IDE with the following steps:
  * Open the Arduino IDE and then build the avr-iot-provision example by navigating
  to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-provision from the menu. 
  * Upload the sketch by using *Sketch->Upload Using Programmer*.
  * Once the board and the IoTConnect account is set up, Load the avr-iot-provision example by navigating
    to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-provision from the menu.
  * Upload the sketch by using *Sketch->Upload Using Programmer*.

## (Optional) Modem Firmware Upgrade

The firmware upgrade has been tested from LR8.0.5.10 to LR8.2.1.0.

If you wish to upgrade the modem firmware, Follow the steps at the 
[AVR-IoT Cellular Mini UserGuide's Sequans Modem section](https://iot.microchip.com/docs/arduino/userguide/sequans_modem)
**carefully**. When purchased, the board will likely come with the older LR8.0.5.10-55042 firmware that supports LTE CAT-M only.
The upgraded firmware will support both CAT-M and NB-IoT, though the software in this repo will only support CAT-M.

Once done, ensure to download and run the [iotprovision tool](https://github.com/microchip-pic-avr-tools/iotprovision-bin/releases)
from Microchip and run it with ```iotprovision -v debug -c aws``` (even for Azure) 

If you have previously configured the board with avr-iot-provision, you must run the sketch again, as the board will lose
the provisioning information.


## Known Issues
* The current version of the SDK receiving devicebound command/OTA messages and sending acknowledgements causes a modem to hang. 
The SDK can only send telemetry messages at this time.
* The SDK avr-iot-provision Sketch supports only LTE CAT-M.

