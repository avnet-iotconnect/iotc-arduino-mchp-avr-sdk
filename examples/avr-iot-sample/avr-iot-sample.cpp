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

#define GENERATED_ID_PREFIX "sn"

static char duid_from_serial_buf[sizeof(GENERATED_ID_PREFIX) + ATCA_SERIAL_NUM_SIZE * 2];


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

static void command_status(const char* ack_id, bool command_success, const char *command_name, const char *message) {
    Log.infof(F("command: %s status=%s: %s\n"), command_name, command_success ? "OK" : "Failed", message);

    iotcl_mqtt_send_cmd_ack(
      ack_id,
      command_success ? IOTCL_C2D_EVT_CMD_SUCCESS_WITH_ACK : IOTCL_C2D_EVT_CMD_FAILED,
      message // allowed to be null, but should not be null if failed, we'd hope
    );
}

static void on_command(IotclC2dEventData data) {
    const char *command = iotcl_c2d_get_command(data);
    const char *ack_id = iotcl_c2d_get_ack_id(data);
    if (NULL != command) {
        if(NULL != strstr(command, "led-user") ) {
            if (NULL != strstr(command, "on")) {
              LedCtrl.on(Led::USER);
            } else {
              LedCtrl.off(Led::USER);
            }
            command_status(ack_id, true, command, "OK");
        } else if(NULL != strstr(command, "led-error") ) {
            if (NULL != strstr(command, "on")) {
              LedCtrl.on(Led::ERROR);
            } else {
              LedCtrl.off(Led::ERROR);
            }
            command_status(ack_id, true, command, "OK");
        } else {
            Log.errorf(F("Unknown command:%s\r\n"), command);
            command_status(ack_id, false, command, "Not implemented");
        }
        free((void *) command);
    } else {
        command_status(ack_id, false, "?", "Internal error");
    }
}

static void on_ota(IotclC2dEventData data) {
    const char *url = iotcl_c2d_get_ota_url(data, 0);
    if (url == NULL) {
      Log.error(F("OTA URL is missing?"));
      return;
    }
    Log.infof(F("OTA download request received for https://%s, but it is not implemented.\n"), url);
}

static void publish_telemetry() {
    IotclMessageHandle msg = iotcl_telemetry_create();

    // Optional. The first time you create a data point, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
    iotcl_telemetry_set_string(msg, "version", APP_VERSION);
    iotcl_telemetry_set_number(msg, "random", rand() % 100);
    iotcl_telemetry_set_number(msg, "temperature", Mcp9808.readTempC());
    iotcl_telemetry_set_number(msg, "light.red", Veml3328.getRed());
    iotcl_telemetry_set_number(msg, "light.green", Veml3328.getGreen());
    iotcl_telemetry_set_number(msg, "light.blue", Veml3328.getBlue());
    iotcl_telemetry_set_number(msg, "light.ir", Veml3328.getIR());
    iotcl_telemetry_set_number(msg, "button_counter", button_press_count);

    iotcl_mqtt_send_telemetry(msg, false);
    iotcl_telemetry_destroy(msg);
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
  Log.setLogLevel(LogLevel::INFO);
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

  IotConnectClientConfig config = {0};
  if (!load_provisioned_data(&config)
    || !config.cpid || 0 == strlen(config.cpid)
    || !config.env || 0 == strlen(config.env)
  ) {
      Log.error("Invalid provisioning data. Please run the avr-iot-provision sketch.");
      return;
  }

  if (!config.duid || 0 == strlen(config.duid)) {
    strcpy(duid_from_serial_buf, GENERATED_ID_PREFIX);
    if (ATCA_SUCCESS != iotc_ecc608_get_serial_as_string(&duid_from_serial_buf[strlen(GENERATED_ID_PREFIX)])) {
      return; // caller will print the error
    }

    config.duid = duid_from_serial_buf;
  }
  Log.infof(F("CPID: %s\r\n"), config.cpid);
  Log.infof(F("ENV : %s\r\n"), config.env);
  Log.infof(F("DUID: %s\r\n"), config.duid);

  if (!connect_lte()) {
      return;
  }
  connected_to_network = true;

  config.ota_cb = on_ota;
  config.status_cb = on_connection_status;
  config.cmd_cb = on_command;
  config.connection_type = IOTC_CT_AZURE; // for now
  config.verbose = true;

  if (iotconnect_sdk_init(&config)) {
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