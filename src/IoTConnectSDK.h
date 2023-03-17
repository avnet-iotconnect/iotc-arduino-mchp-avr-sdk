//
// Copyright: Avnet, Softweb Inc. 2023
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include <stddef.h>
#include <time.h>
#include <Arduino.h>
#include "iotconnect_common.h"
#include "iotconnect_event.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_lib.h"


typedef enum {
    IOTC_CS_UNDEFINED,
    IOTC_CS_MQTT_CONNECTED,
    IOTC_CS_MQTT_DISCONNECTED
} IotConnectConnectionStatus;

typedef void (*IotConnectStatusCallback)(IotConnectConnectionStatus data);

typedef struct {
    char *env;    // Settings -> Key Vault -> CPID.
    char *cpid;   // Settings -> Key Vault -> Evnironment.
    char *duid;   // Name of the device.
    char *dtg;    // DTG of the template.
    char *host;   // IotHub hostname.
    IotclOtaCallback ota_cb; // callback for OTA events.
    IotclCommandCallback cmd_cb; // callback for command events.
    IotclMessageCallback msg_cb; // callback for ALL messages, including the specific ones like cmd or ota callback.
    IotConnectStatusCallback status_cb; // callback for connection status
} IotConnectClientConfig;


IotConnectClientConfig *iotconnect_sdk_init_and_get_config();

// call iotconnect_sdk_init_and_get_config first and configure the SDK before calling iotconnect_sdk_init()
bool iotconnect_sdk_init(void);

bool iotconnect_sdk_is_connected(void);

// Can be used to pass to telemetry functions
IotclConfig *iotconnect_sdk_get_lib_config(void);

// Will check if there are inbound messages and call adequate callbacks if there are any
// This is technically not required for the Paho implementation.
void iotconnect_sdk_receive(void);

// allow mqtt to do work (keepalive and c2d message processing)
void iotconnect_sdk_loop(void);

// data is a null-terminated string
bool iotconnect_sdk_send_packet(const char *data);

void iotconnect_sdk_disconnect(void);


#endif