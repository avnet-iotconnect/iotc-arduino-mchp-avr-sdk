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
#include "IoTConnectSDK.h"
#include "iotc_ecc608.h"
#include "iotc_provisioning.h"

#define APP_VERSION "01.00.00"

#define SerialModule Serial3

static const unsigned long initial_stack_address = SP;

unsigned long get_free_ram () {
    char *const heap_var = (char*) malloc(sizeof(char));
    const unsigned long heap_address = (unsigned long) heap_var;
    free(heap_var);

    return SP - heap_address;
}

unsigned long get_heap_address () {
    char *const heap_var = (char*) malloc(sizeof(char));
    const unsigned long heap_address = (unsigned long) heap_var;
    free(heap_var);
    return heap_address;
}

void get_heap_state (char *buffer, size_t size) {
    const unsigned long heap_address = get_heap_address();
    const unsigned long heap_size = RAMEND - heap_address;
    const long stack_size = initial_stack_address - SP;
    const unsigned long free_ram = get_free_ram();
    const unsigned long sp = SP;
    const unsigned long ramend = RAMEND;
    snprintf(buffer, size, "Heap Usage: %lu / %lu Stack Size: %lu Free RAM: %lu [Stack %lx:%lx Heap %lx:%lx]",
            heap_size, heap_size + free_ram, stack_size, free_ram, initial_stack_address, sp, ramend, heap_address);
}

#define AWS_ID_BUFF_SIZE 130 // normally 41, but just to be on the safe size
#define GENERATED_ID_PREFIX "avr-"
#define DUID_WEB_UI_MAX_LEN 31

static char aws_id_buff[AWS_ID_BUFF_SIZE];
static bool connecteded_to_network = false;

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
    iotcl_telemetry_set_number(msg, "random", rand() % 100);

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
        Log.infof("0x%x\r\n", (unsigned long) ptr);
        if (!ptr) {
            break;
        }
        blocks[i] = ptr;
    }
    Log.infof("====Allocated %d blocks of size %d (of max %d)===\r\n", i, TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    for (int j = 0; j < i; j++) {
        free(blocks[j]);
    }
}
#endif /* MEMORY_TEST */

void demo_setup(void)
{
  Log.infof("Starting the Sample Application %s\r\n", APP_VERSION);  
  delay(200);

  memory_test();

  if (ATCA_SUCCESS != iotc_ecc608_init_provision()) {
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

  { // scope block
  char dbgbuff[200];
  get_heap_state(dbgbuff,200);
  Log.info(dbgbuff);
  }

  Log.infof("CPID: %s\r\n", config->cpid);
  Log.infof("Env : %s\r\n", config->env);
  Log.infof("DUID: %s\r\n", config->duid);

  if (!connect_lte()) {
      return;
  }
  connecteded_to_network = true;

  config->ota_cb = on_ota;
  config->status_cb = on_connection_status;
  config->cmd_cb = on_command;

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
