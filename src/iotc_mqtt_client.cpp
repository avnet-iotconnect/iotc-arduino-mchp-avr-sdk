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

static IotConnectC2dCallback c2d_msg_cb = NULL; // callback for inbound messages
static IotConnectStatusCallback status_cb = NULL; // callback for connection
static IotclSyncResponse* sr;
static bool disconnect_received = false;

void iotc_mqtt_client_disconnect(void) {
    Log.info("Closing the MQTT connection");
    MqttClient.end();
}

bool iotc_mqtt_client_is_connected(void) {
    return MqttClient.isConnected();
}

bool iotc_mqtt_client_send_message(const char *message) {
    return MqttClient.publish(sr->broker.pub_topic, message);
}

void iotc_mqtt_client_loop() {
    if (!sr) {
        Log.error("iotc_mqtt_client_loop(): Client not initialized!");
        return;
    }
    if (!MqttClient.isConnected()) {
        if (status_cb) {
            status_cb(IOTC_CS_MQTT_DISCONNECTED);
            return;
        }
    }
    String message = MqttClient.readMessage(sr->broker.sub_topic);
    // Read message will return an empty string if there were no new
    // messages, so anything other than that means that there was a new
    // message
    if (message != "") {
        Log.infof("Got new message: %s", message.c_str());
        if (c2d_msg_cb) {
            const char* c_str = message.c_str();
            c2d_msg_cb(c_str);
        }
    }

}

static void on_mqtt_connected(void) {
    if (status_cb) {
        status_cb(IOTC_CS_MQTT_CONNECTED);
    }
}
static void on_mqtt_disconnected(void) {
    disconnect_received = true;
    if (status_cb) {
        status_cb(IOTC_CS_MQTT_DISCONNECTED);
    }
}

bool iotc_mqtt_client_init(IotConnectMqttClientConfig *c) {
    disconnect_received = false;
    sr = c->sr;

    if (!c || !sr) {
        Log.error("iotc_mqtt_client_init() called with invalid arguments");
    }

    if (!Lte.isConnected()) {
        Log.error("LTE must be up and running before initializing MQTT");
        return false;
    }

    // Initialize the ECC
    ATCA_STATUS status = ECC608.begin();
    if (status != ATCACERT_E_SUCCESS) {
        Log.error("Could not initialize ECC hardware");
        return false;
    }

    if(!MqttClient.begin(sr->broker.client_id,
        sr->broker.host,
        MQTT_SECURE_PORT,
        true,
        60,
        true,
        sr->broker.user_name,
        "")) {
            Log.errorf("Failed to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                sr->broker.host,
                sr->broker.client_id,
                sr->broker.user_name
            );
            return false;
    }

    MqttClient.onConnectionStatusChange(on_mqtt_connected, on_mqtt_disconnected);

    Log.infof("Connecting to %s ", sr->broker.host);
    int tires_num_500ms = 120; // 60 seconds
    while (!MqttClient.isConnected()) {
        tires_num_500ms--;
        if (tires_num_500ms < 0) {
            Log.raw(""); // start in a new line
            Log.errorf("Timed out while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                sr->broker.host,
                sr->broker.client_id,
                sr->broker.user_name
            );
            MqttClient.end();
            return false;
        }
        if (disconnect_received) {
            Log.errorf("Received a disconnect while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                sr->broker.host,
                sr->broker.client_id,
                sr->broker.user_name
            );
            return false;
        }
        Log.rawf(".");
        delay(500);
    }

    if(!MqttClient.subscribe(sr->broker.sub_topic)) {
        Log.errorf("ERROR: Unable to subscribe for C2D messages topic%s!", sr->broker.sub_topic);
        return false;
    }

    c2d_msg_cb = c->c2d_msg_cb;
    status_cb = c->status_cb;

    if (status_cb) {
        status_cb(IOTC_CS_MQTT_CONNECTED);
    }

    return true;
}
