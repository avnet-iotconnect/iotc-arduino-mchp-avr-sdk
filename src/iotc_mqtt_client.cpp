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
static bool has_c2d_error = false;
static int32_t pending_msg_id = -1;
static uint16_t pending_msg_len;

// Sample topic: "devices/avtds-avr2/messages/devicebound/%24.to=%2Fdevices%2Favtds-avr2%2Fmessages%2FdeviceBound"
// Max device id is 128, tprinted twice + path strings will make it quite long
// TODO: Use malloc/free
static char topicbuff[200];

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
#if 1 // polling does not seem to work
    if (!c || !c->sr) {
        Log.error(F("iotc_mqtt_client_loop(): Client not initialized!"));
        return;
    }
    if (has_c2d_error) {
        has_c2d_error = false;
        // doubtful that increasing polling rate would help, but it's possible
        Log.error("Some c2d messages were lost! Please increase polling (loo) rate.");
    }
    if (!MqttClient.isConnected()) {
        if (c->status_cb) {
            c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
            return;
        }
    }
    if (pending_msg_id != -1) {
        int32_t message_id = pending_msg_id;
        uint16_t message_length = pending_msg_len;
        pending_msg_id = -1;
        pending_msg_len = 0;

        char* msg_buff = (char*) malloc(message_length + 1);
        if (NULL == msg_buff) {
            Log.error(F("Unable to allocate C2D message buffer!"));
        }
        msg_buff[message_length] = 0; // terminate the buffer just in case we don't get a null terminated string
        Log.infof("Topic:%s id:%ld len:%u\r\n", topicbuff, message_id, message_length);
        //topicbuff[0] = 0;

        if (false == MqttClient.readMessage(
            topicbuff, //c->sr->broker.sub_topic, // we only subscribe to one topic
            msg_buff,
            message_length,
            message_id
        )) {
            Log.error(F("Unable to retrieve C2D message!"));
            free(msg_buff);
            return;
        }

        Log.infof(">> %s\r\n", msg_buff);

        if (c->c2d_msg_cb) {
            c->c2d_msg_cb(msg_buff);
        }
        free(msg_buff);
    }
#else
    String message = MqttClient.readMessage("devices/avtds-avr2/messages/devicebound/%24.to=%2Fdevices%2Favtds-avr2%2Fmessages%2FdeviceBound");
    // Read message will return an empty string if there were no new
    // messages, so anything other than that means that there was a new
    // message
    if (message != "") {
        Log.infof("Got new message: %s", message.c_str());
        if (c->c2d_msg_cb) {
            const char* c_str = message.c_str();
            c->c2d_msg_cb(c_str);
        }
    }
#endif
}

static void on_mqtt_connected(void) {
    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_CONNECTED);
    }
}
static void on_mqtt_disconnected(void) {
    disconnect_received = true;
    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_DISCONNECTED);
    }
}

static void on_mqtt_c2d(const char* topic, const uint16_t message_length, const int32_t message_id) {
    if (pending_msg_id != -1) {
        // We did not catch up
        // Afraid to log a message here
        has_c2d_error = true;
        return;
    }
    strcpy(topicbuff, topic);
    pending_msg_id = message_id;
    pending_msg_len = message_length;
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
            Log.errorf("Failed to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            return false;
    }

    MqttClient.onConnectionStatusChange(on_mqtt_connected, on_mqtt_disconnected);

    int tires_num_500ms = 120; // 60 seconds
    while (!MqttClient.isConnected()) {
        tires_num_500ms--;
        if (tires_num_500ms < 0) {
            Log.raw(F("")); // start in a new line
            Log.errorf("Timed out while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            MqttClient.end();
            return false;
        }
        if (disconnect_received) {
            Log.errorf("Received a disconnect while attempting to connect to MQTT using host:%s, client id:%s, username: %s\r\n",
                c->sr->broker.host,
                c->sr->broker.client_id,
                c->sr->broker.user_name
            );
            return false;
        }
        Log.rawf(".");
        delay(500);
    }

    MqttClient.onReceive(on_mqtt_c2d); // set up callback

    if(!MqttClient.subscribe(c->sr->broker.sub_topic, AT_LEAST_ONCE)) {
        Log.errorf("ERROR: Unable to subscribe for C2D messages topic %s!", c->sr->broker.sub_topic);
        return false;
    }


    if (c->status_cb) {
        c->status_cb(IOTC_CS_MQTT_CONNECTED);
    }

    return true;
}
