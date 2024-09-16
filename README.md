## Introduction
IoTConnect C SDK for Microchip AVR Arduino boards like AVR IoT Cellular Mini.

This guide is focusing on developing software with our IoTConnect C SDK.

If you are interested in evaluating and testing this board with IoTConnect,
then you may be interested in trying out our [Quickstart Guide](QUICKSTART.md)

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
under C:\Users\<your username>\Documents\Arduino\libraries\iotconnect-mchp-avr-sdk* in Windows

* Open the Arduino IDE and load the avr-iot-provision example by navigating
to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-provision from the menu. 
* Refer to the the [Quickstart Guide](QUICKSTART.md), but instead of using pre-compiled hex binaries,
build the corresponding samples using the IDE: 
  * Open the Arduino IDE and load the avr-iot-provision example by navigating
  to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-provision from the menu.
  * Run the sketch by selecting the *Sketch->Upload Using Programmer* from the menu,
  and follow instructions from the [Quickstart Guide](QUICKSTART.md). 
  Once this one-time setup is complete, the board will be configured for your IoTConnect account and you 
  can run the avr-iot-sample.
  * Open the Arduino IDE and load the avr-iot-sample example by navigating
  to File->Examples->iotconnect-mchp-avr-sdk->avr-iot-sample from the menu.
  * Run the sketch by selecting the *Sketch->Upload Using Programmer*.

## Known Issues

The current version of the SDK does not support devicebound command messages. The SDK can only send telemetry messages.

