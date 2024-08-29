/* SPDX-License-Identifier: MIT
 * Copyright (C) 2020 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include <stddef.h>
#include <time.h>
#include <Arduino.h>
#include "iotcl.h"


typedef enum {
    IOTC_CS_UNDEFINED,
    IOTC_CS_MQTT_CONNECTED,
    IOTC_CS_MQTT_DISCONNECTED
} IotConnectConnectionStatus;

typedef enum {
    IOTC_CT_UNDEFINED = 0,
    IOTC_CT_AWS,
    IOTC_CT_AZURE
} IotConnectConnectionType;

typedef void (*IotConnectStatusCallback)(IotConnectConnectionStatus data);


typedef struct {
    char *env;    // Settings -> Key Vault -> CPID.
    char *cpid;   // Settings -> Key Vault -> Evnironment.
    char *duid;   // Name of the device.
    IotConnectConnectionType connection_type;
    IotclOtaCallback ota_cb; // callback for OTA events.
    IotclCommandCallback cmd_cb; // callback for command events.
    IotConnectStatusCallback status_cb; // callback for connection status
    bool verbose; // If true, we will output extra info and sent and received MQTT json data to standard out
} IotConnectClientConfig;

// call iotconnect_sdk_init_and_get_config first and configure the SDK before calling iotconnect_sdk_init()
bool iotconnect_sdk_init(IotConnectClientConfig *c);

bool iotconnect_sdk_is_connected(void);

// Will check if there are inbound messages and call adequate callbacks if there are any
// This is technically not required for the Paho implementation.
void iotconnect_sdk_receive(void);

// allow mqtt to do work (keepalive and c2d message processing)
void iotconnect_sdk_loop(void);

void iotconnect_sdk_disconnect(void);


#endif