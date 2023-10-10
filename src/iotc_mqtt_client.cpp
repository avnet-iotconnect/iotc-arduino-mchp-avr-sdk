/* Copyright (C) 2023 Avnet - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */
   
#include <stddef.h>
#include <string.h>
#include <Arduino.h>
#include "log.h"
#include "lte.h"
#include "ecc608.h"
#include "mqtt_client.h"
#include "iotc_mqtt_client.h"

#define MQTT_SECURE_PORT 8883

static bool disconnect_received = false;
static IotConnectMqttClientConfig* c = NULL;

void iotc_mqtt_client_disconnect(void) {
    Log.info(F("Closing the MQTT connection"));
    MqttClient.end();
}

bool iotc_mqtt_client_is_connected(void) {
    return MqttClient.isConnected();
}

bool iotc_mqtt_client_send_message(const char *message) {
    return MqttClient.publish(c->sr->broker.pub_topic, message);
}

void iotc_mqtt_client_loop() {
    if (!c || !c->sr) {
        Log.error(F("iotc_mqtt_client_loop(): Client not initialized!"));
        return;
    }
    if (!MqttClient.isConnected()) {
        if (c->status_cb) {
            c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
            return;
        }
    }
    String message = MqttClient.readMessage(c->sr->broker.sub_topic);
    // Read message will return an empty string if there were no new
    // messages, so anything other than that means that there was a new
    // message
    if (message != "") {
        Log.infof(F("Got new message: %s"), message.c_str());
        if (c->c2d_msg_cb) {
            const char* c_str = message.c_str();
            c->c2d_msg_cb(c_str);
        }
    }
}

static void on_mqtt_disconnected(void) {
    disconnect_received = true;
    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
    }
}

bool iotc_mqtt_client_init(IotConnectMqttClientConfig *config) {
    disconnect_received = false;
    c = config;
    if (!c) {
        Log.error(F("iotc_mqtt_client_init() called with invalid arguments"));
    }

    if (!Lte.isConnected()) {
        Log.error(F("LTE must be up and running before initializing MQTT"));
        return false;
    }

    // Initialize the ECC
    ATCA_STATUS status = ECC608.begin();
    if (status != ATCACERT_E_SUCCESS) {
        Log.error(F("Could not initialize ECC hardware"));
        return false;
    }

    Log.infof("Attempting to connect to MQTT host:%s, client id:%s, username: %s\r\n",
        c->sr->broker.host,
        c->sr->broker.client_id,
        c->sr->broker.user_name
    );

    if(!MqttClient.begin(c->sr->broker.client_id,
        c->sr->broker.host,
        MQTT_SECURE_PORT,
        true,
        60,
        true,
        c->sr->broker.user_name,
        "")) {
            Log.errorf(F("Failed to connect to MQTT using host:%s, client id:%s, username: %s\r\n"),
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            return false;
    }

    MqttClient.onDisconnect(on_mqtt_disconnected);

    int tires_num_500ms = 120; // 60 seconds
    while (!MqttClient.isConnected()) {
        tires_num_500ms--;
        if (tires_num_500ms < 0) {
            Log.raw(F("")); // start in a new line
            Log.errorf(F("Timed out while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n"),
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            MqttClient.end();
            return false;
        }
        if (disconnect_received) {
            Log.errorf(F("Received a disconnect while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n"),
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            return false;
        }
        Log.rawf(F("."));
        delay(500);
    }
    if(!MqttClient.subscribe(c->sr->broker.sub_topic)) {
        Log.errorf(F("ERROR: Unable to subscribe for C2D messages topic %s!"), c->sr->broker.sub_topic);
        return false;
    }

    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_CONNECTED);
    }

    return true;
}
