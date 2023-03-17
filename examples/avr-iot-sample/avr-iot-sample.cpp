/*
  Basic ESP32 MQTT example
  This sketch demonstrates the capabilities of the pubsub library in combination
  with the ESP32 board/library.
  It connects to an MQTT server then:
  - publishes "connected to MQTT" to the topic "outTopic"
  - subscribes to the topic "inTopic", printing out messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the "ON" payload is send to the topic "inTopic" , LED will be turned on, and acknowledgement will be send to Topic "outTopic"
  - If the "OFF" payload is send to the topic "inTopic" , LED will be turned OFF, and acknowledgement will be send to Topic "outTopic"
  It will reconnect to the server if the connection is lost using a
  reconnect function.
*/

#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <log.h>
#include <lte.h>
#include <ecc608.h>

#include <IoTConnectSDK.h>

//
// Copyright: Avnet, Softweb Inc. 2023
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 3/17/23.
//

#define IOTCONNECT_CPID "your-cpid"
#define IOTCONNECT_ENV  "your-env"
#define IOTCONNECT_DUID "your-device-unique-id" // set this to blank string if you want the ID to be automatically generated
#define IOTCONNECT_TEMPLATE_DTG "your-device-template-dtg"
#define IOTCONNECT_HOST "poc-iotconnect-iothub-003-eu2.azure-devices.net" // "ec2-3-80-151-81.compute-1.amazonaws.com" // // "your-iothub-host"

#define APP_VERSION "01.00.00"

static bool connecteded_to_network = false;
static char duid[128];

static void on_connection_status(IotConnectConnectionStatus status) {
    // Add your own status handling
    switch (status) {
        case IOTC_CS_MQTT_CONNECTED:
            Log.info("IoTConnect Client Connected");
            break;
        case IOTC_CS_MQTT_DISCONNECTED:
            Log.info("IoTConnect Client Disconnected");
            break;
        default:
            Log.error("IoTConnect Client ERROR");
            break;
    }
}

static void command_status(IotclEventData data, bool status, const char *command_name, const char *message) {
    const char *ack = iotcl_create_ack_string_and_destroy_event(data, status, message);
    printf("command: %s status=%s: %s\n", command_name, status ? "OK" : "Failed", message);
    printf("Sent CMD ack: %s\n", ack);
    iotconnect_sdk_send_packet(ack);
    free((void *) ack);
}

static void on_command(IotclEventData data) {
    char *command = iotcl_clone_command(data);
    if (NULL != command) {
        free((void *) command);
        command_status(data, false, "?", "Not implemented");
    } else {
        command_status(data, false, "?", "Internal error");
    }
}

static bool is_app_version_same_as_ota(const char *version) {
    return strcmp(APP_VERSION, version) == 0;
}

static bool app_needs_ota_update(const char *version) {
    return strcmp(APP_VERSION, version) < 0;
}

static void on_ota(IotclEventData data) {
    const char *message = NULL;
    char *url = iotcl_clone_download_url(data, 0);
    bool success = false;
    if (NULL != url) {
        printf("Download URL is: %s\n", url);
        const char *version = iotcl_clone_sw_version(data);
        if (is_app_version_same_as_ota(version)) {
            printf("OTA request for same version %s. Sending success\n", version);
            success = true;
            message = "Version is matching";
        } else if (app_needs_ota_update(version)) {
            printf("OTA update is required for version %s.\n", version);
            success = false;
            message = "Not implemented";
        } else {
            printf("Device firmware version %s is newer than OTA version %s. Sending failure\n", APP_VERSION,
                   version);
            // Not sure what to do here. The app version is better than OTA version.
            // Probably a development version, so return failure?
            // The user should decide here.
            success = false;
            message = "Device firmware version is newer";
        }

        free((void *) url);
        free((void *) version);
    } else {
        // compatibility with older events
        // This app does not support FOTA with older back ends, but the user can add the functionality
        const char *command = iotcl_clone_command(data);
        if (NULL != command) {
            // URL will be inside the command
            printf("Command is: %s\n", command);
            message = "Old back end URLS are not supported by the app";
            free((void *) command);
        }
    }
    const char *ack = iotcl_create_ack_string_and_destroy_event(data, success, message);
    if (NULL != ack) {
        printf("Sent OTA ack: %s\n", ack);
        iotconnect_sdk_send_packet(ack);
        free((void *) ack);
    }
}

static void publish_telemetry() {
    IotclMessageHandle msg = iotcl_telemetry_create();

    // Optional. The first time you create a data point, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
    iotcl_telemetry_add_with_iso_time(msg, iotcl_iso_timestamp_now());
    iotcl_telemetry_set_string(msg, "version", APP_VERSION);
    iotcl_telemetry_set_number(msg, "cpu", 3.123); // test floating point numbers

    const char *str = iotcl_create_serialized_string(msg, false);
    iotcl_telemetry_destroy(msg);
    Log.infof("Sending: %s\r\n", str);
    iotconnect_sdk_send_packet(str); // underlying code will report an error
    iotcl_destroy_serialized(str);
}
static void on_lte_disconnect(void) { 
  connecteded_to_network = false; 
}

static bool connect_lte() {
    // Connect with a maximum timeout value of 30 000 ms, if the connection is
    // not up and running within 30 seconds, abort and retry later
    if (!Lte.begin(30000)) {
        Log.warn("Failed to connect to operator.");
        return false;
    } else {
        return true;
    }
}

void demo_setup()
{
  Log.begin(115200);
  Log.infof("Starting the Sample Application %s\r\n", APP_VERSION);  
  delay(1000);

  if (!connect_lte()) {
      return;
  }
  connecteded_to_network = true;

  IotConnectClientConfig *config = iotconnect_sdk_init_and_get_config();

  config->cpid = (char *) IOTCONNECT_CPID;
  config->env = (char *) IOTCONNECT_ENV;
  config->duid = (char *) IOTCONNECT_DUID;
  config->host = (char *) IOTCONNECT_HOST;
  config->dtg = (char *) IOTCONNECT_TEMPLATE_DTG;
  config->ota_cb = on_ota;
  config->status_cb = on_connection_status;
  config->cmd_cb = on_command;

  if (!config->duid || 0 == strlen(config->duid)) {
    // -- Get the thingname and use it to construct the DUID
    uint8_t thingName[128];
    uint8_t thingNameLen = sizeof(thingName);
    ECC608.begin();
    uint8_t err = ECC608.getThingName(thingName, &thingNameLen);
    if (err != ECC608.ERR_OK) {
        Log.error("Could not retrieve thing name from the ECC");
        return;
    }
    if (strlen((char *)thingName) < 20) {
        Log.error("The thing name does not seem to be correct!");
        return;
    }
    strcpy(duid, "avr-");
    // append the last 20 digits of thing name to "avr-"
    strcat(duid, (char*)&thingName[strlen((char*)thingName)-20]);
    config->duid = duid;
    Log.infof("Obtained Device ID from the secure element: %s\r\n", duid);
  }

  if (iotconnect_sdk_init()) {
    Lte.onDisconnect(on_lte_disconnect);
    for (int i = 0; i < 20; i++) {
      if (!connecteded_to_network || !iotconnect_sdk_is_connected()) {
        // not connected, but mqtt should try to reconnect
        break;
      }
      publish_telemetry(); // publish as soon as we detect that button stat changed
      // pause 5 second and "loop" frequently to read incoming mesages
      for (int j = 0; j < 50; j++) {
        iotconnect_sdk_loop();
        delay(100);
      }
    }
  } else {
    Log.error("Encountered an error while initializing the SDK!");
    return;
  }

  iotconnect_sdk_disconnect();
  Lte.end();    
  printf("Done.\n");
}

void demo_loop() {
  // In order to simplify this demo we are doing everything in setup() and controlling our own loop
  delay(1000);
}
