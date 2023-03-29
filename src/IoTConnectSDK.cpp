//
// Copyright: Avnet, Softweb Inc. 2023
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#include <string.h>
#include <Arduino.h>
#include <log.h>
#include "iotconnect_lib.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_discovery.h"
#include "http_get_time.h"
#include "iotc_mqtt_client.h"
#include "IoTConnectSDK.h"


static IotclConfig lib_config = {0};
static IotConnectClientConfig config = {0};

static IotclSyncResponse sr;

static void on_mqtt_c2d_message(const char* message) {
    Log.infof("event>>> %s", message);
    if (!iotcl_process_event(message)) {
        Log.error("Error encountered while processing the message");
    }
}

void iotconnect_sdk_disconnect(void) {
    iotc_mqtt_client_disconnect();
    Log.info("Disconnected.");
}

bool iotconnect_sdk_is_connected(void) {
    return iotc_mqtt_client_is_connected();
}

IotConnectClientConfig *iotconnect_sdk_init_and_get_config() {
    memset(&config, 0, sizeof(config));
    return &config;
}

IotclConfig *iotconnect_sdk_get_lib_config(void) {
    return iotcl_get_config();
}

static void on_message_intercept(IotclEventData data, IotConnectEventType type) {
    switch (type) {
        case ON_FORCE_SYNC:
            Log.info("Got ON_FORCE_SYNC. Disconnecting.");
            iotconnect_sdk_disconnect(); // client will get notification that we disconnected and will reinit

        case ON_CLOSE:
            Log.info("Got a disconnect request. Closing the mqtt connection. Device restart is required.");
            iotconnect_sdk_disconnect();
        default:
            break; // not handling nay other messages
    }

    if (NULL != config.msg_cb) {
        config.msg_cb(data, type);
    }
}

bool iotconnect_sdk_send_packet(const char *data) {
    return iotc_mqtt_client_send_message(data);
}

void iotconnect_sdk_loop(void) {
    return iotc_mqtt_client_loop();
}

static bool generate_sync_response(void) {
    static const char* CLIENT_ID_FORMAT = "%s-%s";
    static const char* USERNAME_FORMAT = "%s/%s/?api-version=2018-06-30";
    static const char* PUB_TOPIC_FORMAT = "devices/%s/messages/events/";
    static const char* SUB_TOPIC_FORMAT = "devices/%s/messages/devicebound/#";

    if (sr.cpid) {
        Log.info("Sync response already generated.");
    }

    sr.broker.client_id = (char *) malloc(strlen(CLIENT_ID_FORMAT) + strlen(config.cpid) + strlen(config.duid));
    sprintf(sr.broker.client_id, CLIENT_ID_FORMAT, config.cpid, config.duid);

    sr.broker.user_name = (char *) malloc(strlen(USERNAME_FORMAT) + strlen(config.host) + strlen(sr.broker.client_id));
    sprintf(sr.broker.user_name, USERNAME_FORMAT, config.host, sr.broker.client_id);

    sr.broker.pub_topic = (char *) malloc(strlen(PUB_TOPIC_FORMAT) + strlen(sr.broker.client_id));
    sprintf(sr.broker.pub_topic, PUB_TOPIC_FORMAT, sr.broker.client_id);

    sr.broker.sub_topic = (char *) malloc(strlen(SUB_TOPIC_FORMAT) + strlen(sr.broker.client_id));
    sprintf(sr.broker.sub_topic, SUB_TOPIC_FORMAT, sr.broker.client_id);

    if (!sr.broker.client_id || !sr.broker.user_name || ! sr.broker.pub_topic || !sr.broker.sub_topic) {
        Log.error("generate_sync_response: Allocation error.");
        return false;
    }

    sr.ds = IOTCL_SR_OK;
    sr.cpid = config.cpid;
    sr.dtg = config.dtg;
    sr.broker.host = config.host;
    return true;
}


///////////////////////////////////////////////////////////////////////////////////
// this the Initialization os IoTConnect SDK
bool iotconnect_sdk_init(void) {
    if (!generate_sync_response()) {
        return false;
    }
    // We want to print only first 4 characters of cpid
    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;

    Log.infof("Time now: %lu\r\n", http_get_time());

    if (!config.env || !config.cpid || !config.duid || !config.dtg || !config.host) {
        Log.error("Error: Device configuration is invalid. Configuration values for env, cpid and duid are required.");
        return false;
    }

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = on_message_intercept;

    lib_config.telemetry.dtg = sr.dtg;

    char cpid_buff[5];
    strncpy(cpid_buff, config.cpid, 4);
    cpid_buff[4] = 0;
    Log.infof("CPID: %s***\r\n", cpid_buff);
    Log.infof("ENV:  %s\r\n", config.env);

    if (!iotcl_init(&lib_config)) {
        Log.error("Error: Failed to initialize the IoTConnect Lib");
        return false;
    }

    IotConnectMqttClientConfig mqtt_config;
    mqtt_config.sr = &sr;
    mqtt_config.status_cb = config.status_cb;
    mqtt_config.c2d_msg_cb = on_mqtt_c2d_message;

    if (!iotc_mqtt_client_init(&mqtt_config)) {
        Log.error("Failed to connect!");
        return false;
    }

    return true;
}