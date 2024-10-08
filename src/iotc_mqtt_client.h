/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#ifndef IOTC_MQTT_CLIENT_H
#define IOTC_MQTT_CLIENT_H

#include <Arduino.h>
#include "iotconnect.h"


typedef void (*IotConnectC2dCallback)(const char* message);

typedef struct {
    IotConnectC2dCallback c2d_msg_cb; // callback for inbound messages
    IotConnectStatusCallback status_cb; // callback for connection status
} IotConnectMqttClientConfig;

bool iotc_mqtt_client_init(IotConnectMqttClientConfig *c);

void iotc_mqtt_client_disconnect(void);

bool iotc_mqtt_client_is_connected(void);

void iotc_mqtt_client_loop(void);

bool iotc_mqtt_client_send_message(const char *topic, const char *message);

#endif // IOTC_MQTT_CLIENT_H