/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#include <Arduino.h>
#include "log.h"
#include "cryptoauthlib/app/tng/tng_atcacert_client.h"
#include "iotc_ecc608.h"
#include "iotc_provisioning.h"
#include "iotconnect.h"

// for user input we are using raw serial
#define SerialModule Serial3
#define ASCII_CARRIAGE_RETURN (0xD)
#define ASCII_LINE_FEED       (0xA)
#define ASCII_BACKSPACE       (0x8)
#define ASCII_DELETE          (0x7F)
#define ASCII_SPACE           (0x20)

#define GENERATED_ID_PREFIX "sn"

static char duid_from_serial_buf[sizeof(GENERATED_ID_PREFIX) + ATCA_SERIAL_NUM_SIZE * 2];

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
  bool ret = 0 != strlen(config->cpid) && 0 != strlen(config->env);
  if (!ret) {
    Log.info("Device does not have the configuration in storage. Please set up the device.");
  }
  return ret;
}

// function reused from Mircochip's provision.ino
static bool read_until_newline(char* output_buffer, const size_t output_buffer_length) {
    size_t index = 0;

    output_buffer[0] = 0;

    while (true) {
        while (!SerialModule.available()) {}
        uint8_t input = SerialModule.read();

        if (input == ASCII_SPACE && index == 0) {
            continue;
        }
        if (input == ASCII_CARRIAGE_RETURN || input == ASCII_LINE_FEED) {
            break;
        } else if (input == ASCII_BACKSPACE || input == ASCII_DELETE) {
            if (index > 0) {
                // Prevent removing the printed text, only remove if the
                // user has inputted some text
                SerialModule.print((char)input);
                output_buffer[index] = '\0';
                index--;
            }
            continue;
        }
        SerialModule.print((char)input);
        // - 1 here since we need space for null termination
        if (index >= output_buffer_length - 1) {
            Log.errorf("Reached maximum input length of %d", (int) output_buffer_length);
            return false;
        }
        output_buffer[index++] = input;
        // Fill null termination as we go
        output_buffer[index] = '\0';
    }
    return true;
}

static bool validate_user_input(const char* value, bool required, bool dash_allowed) {
  if (0 == strlen(value)) {
    if (required) {
      Log.info("This value is required. Please try again.");
      return false;
    } else {
      return true; // field is not required, so it can be empty
    }
  }
  for (size_t i = 0; i < strlen(value); i++) {
    char ch = value[i];
    bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (dash_allowed && ch == '-');
    if (!valid) {
      Log.infof("Invalid character '%c' with ASCII code 0x%x encountered in input.\n", ch, ch);
      Log.info("Please check your input try again...");
      return false;
    }
  }
  return true; // all characters are valid and input is non-empty
}

static void print_string_or_default(const char* item, const char* value, const char* default_value) {
  if (!value || 0 == strlen(value)) {
    Log.rawf(F("%s: %s\n"), item, default_value);
  } else {
    Log.rawf(F("%s: %s\n"), item, value);
  }
}

static bool provision_from_user_input(IotConnectClientConfig *config) {
  const char* const PLATFORM_AWS = "aws";
  const char* const PLATFORM_AZ = "az";
  IotConnectConnectionType platform;

  char platform_buff[5]; // aws or az
  char cpid_buff[IOTC_ECC608_PROV_CPID_SIZE];
  char env_buff[IOTC_ECC608_PROV_ENV_SIZE];
  char duid_buff[IOTC_ECC608_PROV_DUID_SIZE];

  Log.raw(F(" Please enter your IoTConnect device configuration."));
  Log.rawf(F(" If DUID is empty, the following deivce ID will be used: %s\n"), duid_from_serial_buf);


  do {
    Log.rawf(F("\nPlatform AWS or Azure (\"aws\" or \"az\"): "));

  } while(!read_until_newline(platform_buff, 4)
    || ((0 != strcmp(platform_buff, PLATFORM_AWS)) && (0 != strcmp(platform_buff, PLATFORM_AZ)))
  );

  if (0 == strcmp(platform_buff, PLATFORM_AWS)) {
    platform = IOTC_CT_AWS;
  } else {
    platform = IOTC_CT_AZURE;
  }

  do {
    Log.rawf(F("\nCPID: "));
  } while(!read_until_newline(cpid_buff, IOTC_ECC608_PROV_CPID_SIZE - 1)
    || !validate_user_input(cpid_buff, true, false));
  do {
    Log.rawf(F("\nEnvironemnt: "));
      ; // do forever
  } while(!read_until_newline(env_buff, IOTC_ECC608_PROV_ENV_SIZE - 1)
    || !validate_user_input(env_buff, true, false));

  do {
    Log.rawf(F("\nDUID: "));
  } while(!read_until_newline(duid_buff, IOTC_ECC608_PROV_DUID_SIZE - 1)
    || !validate_user_input(duid_buff, false, true));

  Log.rawf(F("\n--------------------\n")); //needs a new line at beginning and
  if(ATCA_SUCCESS != iotc_ecc608_set_string_value(IOTC_ECC608_PROV_CPID, cpid_buff)) {
    Log.error("Error while storing the CPID value.");
    return false; // called function will also print a relevant error
  }

  if(ATCA_SUCCESS != iotc_ecc608_set_platform(platform)) {
    Log.error("Error while storing the Environment value.");
    return false; // called function will also print a relevant error
  }
  if(ATCA_SUCCESS != iotc_ecc608_set_string_value(IOTC_ECC608_PROV_ENV, env_buff)) {
    Log.error("Error while storing the Environment value.");
    return false; // called function will also print a relevant error
  }
  if(ATCA_SUCCESS != iotc_ecc608_set_string_value(IOTC_ECC608_PROV_DUID, duid_buff)) {
    Log.error("Error while storing the DUID value.");
    return false; // called function will also print a relevant error
  }
  if(ATCA_SUCCESS != iotc_ecc608_write_all_data()) {
    return false; // called function will print a relevant error
  }

  // iotc_ecc608 module will keep the pointers around for us
  if(ATCA_SUCCESS != iotc_ecc608_get_platform(&(config->connection_type))
    || ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_CPID, &(config->cpid))
    || ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_ENV, &(config->env))
    || ATCA_SUCCESS != iotc_ecc608_get_string_value(IOTC_ECC608_PROV_DUID, &(config->duid))
  ) {
    return false;
  }

  return true;
}

void setup() {
  Log.begin(115200);
  Log.setLogLevel(LogLevel::INFO);

  Log.info("Starting the provisioning sample...");

  iotc_prov_init();

  if (ATCA_SUCCESS != iotc_ecc608_init_provision()) {
    return; // caller will print the error
  }

  iotc_prov_print_device_certificate();

  strcpy(duid_from_serial_buf, GENERATED_ID_PREFIX);
  if (ATCA_SUCCESS != iotc_ecc608_get_serial_as_string(&duid_from_serial_buf[strlen(GENERATED_ID_PREFIX)])) {
    return; // caller will print the error
  }

  IotConnectClientConfig config = {0};
  if (!load_provisioned_data(&config)) {
      Log.info("Invalid provisioning data. Please provide the device IoTConnect configuraton.");
  }

  Log.rawf(F("\nCurrent provisioning data:\n"));
  Log.rawf(F("Platform\t: %s\n"), config.connection_type == IOTC_CT_AZURE ? "Azure" : "AWS");
  print_string_or_default("CPID\t\t", config.cpid, "[EMPTY]");
  print_string_or_default("Environment\t", config.env, "[EMPTY]");
  print_string_or_default("Device Uinque ID", config.duid, duid_from_serial_buf);

  while (!provision_from_user_input(&config)) {};

  if (!iotc_prov_setup_tls_and_server_certs(config.connection_type)) {
    Log.error("TLS provisioning failed");
    return; // called function will print errors
  }

  Log.raw(F("New provisioning data:"));
  Log.rawf(F("Platform: %s\n"), config.connection_type == IOTC_CT_AZURE ? "Azure" : "AWS");
  print_string_or_default("CPID", config.cpid, "[EMPTY]");
  print_string_or_default("Environment", config.env, "[EMPTY]");
  print_string_or_default("Device Uinque ID", config.duid, duid_from_serial_buf);

  Log.info("========= Provisioning Complete =========");
}

void loop() {
  delay(10000);
}
