//
// Copyright: Avnet, Softweb Inc. 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 3/17/23.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduino.h>
#include "log.h"
#include "lte.h"
#include "mcp9808.h"
#include "mqtt_client.h"
#include "led_ctrl.h"
#include "veml3328.h"
#include "IoTConnectSDK.h"
#include "iotc_ecc608.h"
#include "iotc_provisioning.h"

#define APP_VERSION "02.00.00"

#define AWS_ID_BUFF_SIZE 130 // normally 41, but just to be on the safe size
#define GENERATED_ID_PREFIX "avr-"
#define DUID_WEB_UI_MAX_LEN 31

static char aws_id_buff[AWS_ID_BUFF_SIZE];
static bool connected_to_network = false;
static bool button_pressed = false;
static int button_press_count = 0;

static void pd2_button_interrupt(void) {
    if (PORTD.INTFLAGS & PIN2_bm) {
        PORTD.INTFLAGS = PIN2_bm;
        button_pressed = true;
        button_press_count++;
    }
}

static void on_connection_status(IotConnectConnectionStatus status) {
    // Add your own status handling
    switch (status) {
        case IOTC_CS_MQTT_CONNECTED:
            Log.info(F("IoTConnect Client Connected"));
            break;
        case IOTC_CS_MQTT_DISCONNECTED:
            Log.info(F("IoTConnect Client Disconnected"));
            break;
        default:
            Log.error(F("IoTConnect Client ERROR"));
            break;
    }
}

static void command_status(IotclEventData data, bool status, const char *command_name, const char *message) {
    const char *ack = iotcl_create_ack_string_and_destroy_event(data, status, message);
    Log.infof(F("command: %s status=%s: %s\n"), command_name, status ? "OK" : "Failed", message);
    Log.infof(F("Sent CMD ack: %s\n"), ack);
    iotconnect_sdk_send_packet(ack);
    free((void *) ack);
}

static void on_command(IotclEventData data) {
    char *command = iotcl_clone_command(data);
    if (NULL != command) {
        if(NULL != strstr(command, "led-user") ) {
            if (NULL != strstr(command, "on")) {
              LedCtrl.on(Led::USER);
            } else {
              LedCtrl.off(Led::USER);
            }
            command_status(data, true, command, "OK");
        } else if(NULL != strstr(command, "led-error") ) {
            if (NULL != strstr(command, "on")) {
              LedCtrl.on(Led::ERROR);
            } else {
              LedCtrl.off(Led::ERROR);
            }
            command_status(data, true, command, "OK");
        } else {
            Log.errorf(F("Unknown command:%s\r\n"), command);
            command_status(data, false, command, "Not implemented");
        }
        free((void *) command);
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
        Log.infof(F("Download URL is: %s\n"), url);
        const char *version = iotcl_clone_sw_version(data);
        if (is_app_version_same_as_ota(version)) {
            Log.infof(F("OTA request for same version %s. Sending success\n"), version);
            success = true;
            message = "Version is matching";
        } else if (app_needs_ota_update(version)) {
            Log.infof(F("OTA update is required for version %s.\n"), version);
            success = false;
            message = "Not implemented";
        } else {
            Log.infof(F("Device firmware version %s is newer than OTA version %s. Sending failure\n"), APP_VERSION,
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
            Log.infof(F("Command is: %s\n"), command);
            message = "Old back end URLS are not supported by the app";
            free((void *) command);
        }
    }
    const char *ack = iotcl_create_ack_string_and_destroy_event(data, success, message);
    if (NULL != ack) {
        Log.infof(F("Sent OTA ack: %s\n"), ack);
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
    iotcl_telemetry_set_number(msg, "random", rand() % 100);
    iotcl_telemetry_set_number(msg, "temperature", Mcp9808.readTempC());
    iotcl_telemetry_set_number(msg, "light.red", Veml3328.getRed());
    iotcl_telemetry_set_number(msg, "light.green", Veml3328.getGreen());
    iotcl_telemetry_set_number(msg, "light.blue", Veml3328.getBlue());
    iotcl_telemetry_set_number(msg, "light.ir", Veml3328.getIR());
    iotcl_telemetry_set_number(msg, "button_counter", button_press_count);

    const char *str = iotcl_create_serialized_string(msg, false);
    iotcl_telemetry_destroy(msg);
    Log.infof(F("Sending: %s\r\n"), str);
    iotconnect_sdk_send_packet(str); // underlying code will report an error
    iotcl_destroy_serialized(str);
}

static void on_lte_disconnect(void) {
  connected_to_network = false;
}

static bool connect_lte() {
    // Connect with a maximum timeout value of 30 000 ms, if the connection is
    // not up and running within 30 seconds, abort and retry later
    if (!Lte.begin()) {
        return false;
    } else {
        return true;
    }
}

static bool load_provisioned_data(IotConnectClientConfig *config) {
  if (ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_CPID, &(config->cpid))) {
    return false; // caller will print the error
  }
  if (ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_ENV, &(config->env))) {
    return false; // caller will print the error
  }
  if (ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_DUID, &(config->duid))) {
    return false; // caller will print the error
  }
  return true;
}

#define MEMORY_TEST
#ifdef MEMORY_TEST
#define TEST_BLOCK_SIZE  1 * 128
#define TEST_BLOCK_COUNT 30
void memory_test() {
    void *blocks[TEST_BLOCK_COUNT];
    int i = 0;
    for (; i < TEST_BLOCK_COUNT; i++) {
        void *ptr = malloc(TEST_BLOCK_SIZE);
        Log.infof(F("0x%x\r\n"), (unsigned long) ptr);
        blocks[i] = ptr;
        if (!ptr) {
            break;
        }
    }
    Log.infof(F("====Allocated %d blocks of size %d (of max %d)===\r\n"), i, TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    for (int j = 0; j < i; j++) {
        free(blocks[j]);
    }
}
#endif /* MEMORY_TEST */

#define RESERVE_BLOCK_SIZE  128
#define RESERVE_BLOCK_COUNT 30
#define RESERVE_LEAK_COUNT 5 // Leak the last X blocks
void reserve_stack_with_heap_leak() {
    void *blocks[RESERVE_BLOCK_COUNT];
    int i = 0;
    for (; i < RESERVE_BLOCK_COUNT; i++) {
        void *ptr = malloc(RESERVE_BLOCK_SIZE);
        blocks[i] = ptr;
        if (!ptr) {
            break;
        }
    }
    Log.infof(F("====Allocated %d blocks of size %d (of max %d)===\r\n"), i, RESERVE_BLOCK_SIZE, RESERVE_BLOCK_COUNT);
    for (int j = 0; j < (i - RESERVE_LEAK_COUNT); j++) {
        free(blocks[j]);
    }
}

void demo_setup(void)
{
  Log.begin(115200);
  Log.setLogLevel(LogLevel::DEBUG);
  delay(2000);

  Log.infof(F("Starting the Sample Application %s\r\n"), APP_VERSION);

  LedCtrl.begin();
  LedCtrl.startupCycle();

  if (Mcp9808.begin()) {
      Log.error(F("Could not initialize the temperature sensor"));
  }
  if (Veml3328.begin()) {
      Log.error(F("Could not initialize the light sensor"));
  }

  // Set PD2 as input (button)
  pinConfigure(PIN_PD2, PIN_DIR_INPUT | PIN_PULLUP_ON);
  attachInterrupt(PIN_PD2, pd2_button_interrupt, FALLING);

  if (ATCA_SUCCESS != iotc_ecc608_init_provision()) {
    Log.error(F("Failed to read provisioning data!"));
    delay(10000);
    return; // caller will print the error
  }

  IotConnectClientConfig *config = iotconnect_sdk_init_and_get_config();
  if (!load_provisioned_data(config)
    || !config->cpid || 0 == strlen(config->cpid)
    || !config->env || 0 == strlen(config->env)
  ) {
      Log.error("Invalid provisioning data. Please run the avr-iot-provision sketch.");
      return;
  }

  if (!config->duid || 0 == strlen(config->duid)) {
    strcpy(aws_id_buff, GENERATED_ID_PREFIX);
    char* bufer_location = &aws_id_buff[strlen(GENERATED_ID_PREFIX)];
    size_t buffer_size = AWS_ID_BUFF_SIZE - strlen(GENERATED_ID_PREFIX);
    if (ATCA_SUCCESS != iotc_ecc608_copy_string_value(AWS_THINGNAME, bufer_location, buffer_size)) {
      return; // caller will print the error
    }
    // terminate for max length
    aws_id_buff[DUID_WEB_UI_MAX_LEN] = 0;

    config->duid = aws_id_buff;
  }
  Log.infof(F("CPID: %s\r\n"), config->cpid);
  Log.infof(F("ENV : %s\r\n"), config->env);
  Log.infof(F("DUID: %s\r\n"), config->duid);

  if (!connect_lte()) {
      return;
  }
  connected_to_network = true;

  config->ota_cb = on_ota;
  config->status_cb = on_connection_status;
  config->cmd_cb = on_command;

  if (iotconnect_sdk_init()) {
    Lte.onDisconnect(on_lte_disconnect);

    // MAIN TELEMETRY LOOP - X messages
    for (int i = 0; i < 50; i++) {
      if (!connected_to_network || !iotconnect_sdk_is_connected()) {
        break;
      }
      publish_telemetry(); // publish as soon as we detect that button stat changed

      // DELAY / SDK POLL LOOP - X iteratins of SDK loop 2-second delays
      // note that iotconnect_sdk_loop() takes 2 seconds to complete, so there's no real need for delay()
      for (int j = 0; j < 30; j++) {
        if (!connected_to_network || !iotconnect_sdk_is_connected()) {
          break;
        }
       iotconnect_sdk_loop(); // loop will take 2 seconds to complete (related to the modem polling most likely)
       delay(2000);
        if (button_pressed) {
          button_pressed = false;
          LedCtrl.startupCycle(); // first publish once then flicker leds
          // BURST LOOP send messages in quick succession?
          for (int burst_count = 0; burst_count < 20; burst_count++) {
            publish_telemetry(); // publish as soon as we detect that button stat changed
            iotconnect_sdk_loop(); // loop will take 2 seconds to complete (related to the modem polling most likely)
            delay(2000);
          }
        }
      }
    }
  } else {
    Log.error(F("Encountered an error while initializing the SDK!"));
    return;
  }

  iotconnect_sdk_disconnect();
  Lte.end();
  Log.infof(F("Done.\n"));
}

void demo_loop() {
  // In order to simplify this demo we are doing everything in setup() and controlling our own loop
  delay(1000);
}