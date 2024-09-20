/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
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
#define IOTC_MQTT_CONN_RETRY_INTERVAL_MS      5000
#define IOTC_MAX_MQTT_CONN_RETRIES            3

static bool disconnect_received = false;
static IotConnectMqttClientConfig* c = NULL;

typedef struct {
    bool has_message;
    char topic[MQTT_TOPIC_MAX_LENGTH];
    uint16_t message_length;
    int32_t message_id;
} IotcMqttLastC2dMessage;

static IotcMqttLastC2dMessage last_c2d_message = {0};


// NOTE: Will run in ISR. Not in real use temporarily.
// TODO: Handle this and implement offloading the message from the ISR
static void iotc_mqtt_client_on_receive(
    const char* topic,
    const uint16_t message_length,
    const int32_t message_id) {
        if (Log.getLogLevel() == LogLevel::DEBUG) {
            char buff[10];
            // don't want to printf (fromatted) in ISR
            Log.infof(F("Message received on topic: "));
            Log.rawf(topic);
            Log.infof(F(" len:"));
            itoa(message_length, buff, 10);
            Log.rawf(buff);
            Log.rawf(F(" id:"));
            itoa(message_id, buff, 10);
            Log.raw(buff);
        }
        if (last_c2d_message.has_message) {
            Log.warn(F("Previous C2D message was not processed. Consider increasing the frequency of calls to iotconnect_sdk_loop() or reducing C2D messaging rate"));
        }
        strcpy(last_c2d_message.topic, topic);
        last_c2d_message.message_id = message_id;
        last_c2d_message.message_length = message_length;
        last_c2d_message.has_message = true;
}

static void on_mqtt_disconnected(void) {
    disconnect_received = true;
    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
    }
}

void iotc_mqtt_client_disconnect(void) {
    Log.info(F("Closing the MQTT connection"));
    MqttClient.end();
}

bool iotc_mqtt_client_is_connected(void) {
    return MqttClient.isConnected();
}

bool iotc_mqtt_client_send_message(const char* topic, const char *message) {
    return MqttClient.publish(topic, message, AT_LEAST_ONCE);
}

void iotc_mqtt_client_loop() {
    if (!MqttClient.isConnected()) {
        if (c->status_cb) {
            c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
            return;
        }
    }

    if (last_c2d_message.has_message) {
        last_c2d_message.has_message = false;
        char data_buffer[last_c2d_message.message_length + 1] = {0};
        if (0 == last_c2d_message.message_id) {
            // BUG: if QOS is zero, the AVR IoT library returns 0 instead of -1.
            // This causes the fetch with msg_id = 0 to fail.
            last_c2d_message.message_id = -1;
        }
        MqttClient.readMessage(
            last_c2d_message.topic,
            data_buffer,
            last_c2d_message.message_length + 1,
            last_c2d_message.message_id
        );
        data_buffer[last_c2d_message.message_length] = '\0'; // terminate the string, just in case
        c->c2d_msg_cb(data_buffer);
    }

#if 0
    // Receiving messages seems to be the main loop into a stuck state.
    // We need to investigate this further...
    // Known issue for now.
    IotclMqttConfig* mc = iotcl_mqtt_get_config();
    if (!mc) {
        Log.error(F("iotc_mqtt_client_loop(): Error: iotcl not configured?!"));
    	return; // called function will print the error
    }
    String message = MqttClient.readMessage(mc->sub_c2d);
    // Read message will return an empty string if there were no new
    // messages, so anything other than that means that there was a new
    // message
    if (message != "") {
        Log.infof(F("Got new message: %s\n"), message.c_str());
        if (c->c2d_msg_cb) {
            const char* c_str = message.c_str();
            c->c2d_msg_cb(c_str);
        }
    }
#endif
}

bool iotc_mqtt_client_connect(void) {
    IotclMqttConfig* mc = iotcl_mqtt_get_config();
    for (int i = 0; i < IOTC_MAX_MQTT_CONN_RETRIES; i++) {
        if(MqttClient.begin(mc->client_id,
            mc->host,
            MQTT_SECURE_PORT,
            true,
            60,
            true,
            mc->username ? mc->username : "",
            "",
            30000)) {
            return true;
        } else {
            unsigned int backoff = (rand() % IOTC_MQTT_CONN_RETRY_INTERVAL_MS) + 1000; // minimum 1 second
            Log.errorf(F("Failed to connect to MQTT using host:%s, client id:%s, username:%s. Retrying in %d ms\n"),
                mc->host,
                mc->client_id,
                mc->username ? mc->username : "[empty]",
                backoff
            );
            MqttClient.end();
            delay(backoff);
        }
    }
    Log.errorf(F("Failed to connect to MQTT after %d retries\n"), IOTC_MAX_MQTT_CONN_RETRIES);
    return false;
}

bool iotc_mqtt_client_init(IotConnectMqttClientConfig *config) {
    disconnect_received = false;
    c = config;

    IotclMqttConfig* mc = iotcl_mqtt_get_config();

    if (!mc) {
        Log.error(F("iotc_mqtt_client_init() c-lib not initialized?"));
    	return false;
    }
    if (!c) {
        Log.error(F("iotc_mqtt_client_init() called with invalid arguments"));
        return false;
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

    Log.infof("Attempting to connect to MQTT host:%s, client id:%s, username:%s\n",
        mc->host,
        mc->client_id,
        mc->username ? mc->username : "[empty]"
    );


    MqttClient.onDisconnect(on_mqtt_disconnected);

    if (!iotc_mqtt_client_connect()) {
        return false;
    }

    int tires_num_500ms = 120; // 60 seconds
    while (!MqttClient.isConnected()) {
        tires_num_500ms--;
        if (tires_num_500ms < 0) {
            Log.raw(F("")); // start in a new line
            Log.errorf(F("Timed out while attempting to connect to MQTT using host:%s, client id:%s, username:%s\n"),
                mc->host,
                mc->client_id,
                mc->username ? mc->username : "[empty]"
            );
            MqttClient.end();
            return false;
        }
        if (disconnect_received) {
            Log.errorf(F("Received a disconnect while attempting to connect to MQTT using host:%s, client id:%s, username:%s\n"),
                mc->host,
                mc->client_id,
                mc->username ? mc->username : "[empty]"
            );
            return false;
        }
        Log.rawf(F("_"));
        delay(500);
    }

    MqttClient.onReceive(iotc_mqtt_client_on_receive);


    if(!MqttClient.subscribe(mc->sub_c2d, AT_LEAST_ONCE)) {
        Log.errorf(F("ERROR: Unable to subscribe for C2D messages topic %s!\n"), mc->sub_c2d);
        return false;
    }

    return true;
}
