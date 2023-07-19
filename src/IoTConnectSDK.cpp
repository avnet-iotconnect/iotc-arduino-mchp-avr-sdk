//
// Copyright: Avnet, Softweb Inc. 2023
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#include <string.h>
#include <Arduino.h>
#include "log.h"
#include "iotconnect_lib.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_discovery.h"
#include "iotc_time.h"
#include "iotc_http_request.h"
#include "iotc_mqtt_client.h"
#include "IoTConnectSDK.h"

#define HTTP_DISCOVERY_PATH_FORMAT "/api/sdk/cpid/%s/lang/M_C/ver/2.0/env/%s"

static IotclConfig lib_config = {0};
static IotConnectClientConfig config = {0};
static IotConnectMqttClientConfig mqtt_config = {0};

static void dump_response(const char *message, IotConnectHttpResponse *response) {
    Log.infof("%s:\r\n", message);
    if (response->data) {
        Log.infof(" Response was:\r\n----\r\n%s\r\n----\r\n", response->data);
    } else {
        Log.infof(" Response was empty\r\n");
    }
}

static void report_sync_error(IotclSyncResponse *response, const char *sync_response_str) {
    if (NULL == response) {
        Log.error(F("Failed to obtain sync response"));
        return;
    }
    switch (response->ds) {
        case IOTCL_SR_DEVICE_NOT_REGISTERED:
            Log.error(F("IOTC_SyncResponse error: Not registered"));
            break;
        case IOTCL_SR_AUTO_REGISTER:
            Log.error(F("IOTC_SyncResponse error: Auto Register"));
            break;
        case IOTCL_SR_DEVICE_NOT_FOUND:
            Log.error(F("IOTC_SyncResponse error: Device not found"));
            break;
        case IOTCL_SR_DEVICE_INACTIVE:
            Log.error(F("IOTC_SyncResponse error: Device inactive"));
            break;
        case IOTCL_SR_DEVICE_MOVED:
            Log.error(F("IOTC_SyncResponse error: Device moved"));
            break;
        case IOTCL_SR_CPID_NOT_FOUND:
            Log.error(F("IOTC_SyncResponse error: CPID not found"));
            break;
        case IOTCL_SR_UNKNOWN_DEVICE_STATUS:
            Log.error(F("IOTC_SyncResponse error: Unknown device status error from server"));
            break;
        case IOTCL_SR_ALLOCATION_ERROR:
            Log.error(F("IOTC_SyncResponse internal error: Allocation Error"));
            break;
        case IOTCL_SR_PARSING_ERROR:
            Log.error(F("IOTC_SyncResponse internal error: Parsing error. Please check parameters passed to the request."));
            break;
        default:
            Log.warn(F("WARN: report_sync_error called, but no error returned?"));
            break;
    }
    Log.errorf("Raw server response was:\r\n--------------\r\n%s\r\n--------------\r\n", sync_response_str);
}

static IotclDiscoveryResponse *run_http_discovery(const char *cpid, const char *env) {
    char *json_start = NULL;
    IotclDiscoveryResponse *ret = NULL;
    char *path_buff = (char *) malloc(sizeof(HTTP_DISCOVERY_PATH_FORMAT) +
                            strlen(cpid) +
                            strlen(env)/* %s x 2 */
    );

    sprintf(path_buff, HTTP_DISCOVERY_PATH_FORMAT, cpid, env);

    IotConnectHttpResponse response = {0};

    iotconnect_https_request(
        &response,
        IOTCONNECT_DISCOVERY_HOSTNAME,
        path_buff,
        NULL
    );

    free(path_buff);

    if (NULL == response.data) {
        dump_response("Unable to parse HTTP response", &response);
        goto cleanup;
    }
    json_start = strstr(response.data, "{");
    if (NULL == json_start) {
        dump_response("No json response from server", &response);
        goto cleanup;
    }
    if (json_start != response.data) {
        dump_response("WARN: Expected JSON to start immediately in the returned data", &response);
    }

    ret = iotcl_discovery_parse_discovery_response(json_start);
    if (!ret) {
        Log.errorf("Error: Unable to get discovery response for environment \"%s\"."
            "Please check the environment name in the key vault.\r\n",
            env
        );
    }

    // fall through
    cleanup:
    iotconnect_free_https_response(&response);
    return ret;
}

static IotclSyncResponse *run_http_sync(IotclDiscoveryResponse* dr, const char *cpid, const char *uniqueid) {
    char *json_start = NULL;
    IotclSyncResponse *ret = NULL;
    char *post_data = (char *)malloc(IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_MAX_LEN + 1);

    if (!post_data) {
        Log.error(F("run_http_sync: Out of memory!"));
        return NULL;
    }

    snprintf(post_data,
             IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_MAX_LEN, /*total length should not exceed MTU size*/
             IOTCONNECT_DISCOVERY_PROTOCOL_POST_DATA_TEMPLATE,
             cpid,
             uniqueid
    );

    char * path = (char *) malloc(strlen(dr->path) + 5 /* "sync?" */ + 1/* null*/);
    strcpy(path, dr->path);
    strcat(path, "sync?");
    IotConnectHttpResponse response = {0};
    iotconnect_https_request(
        &response,
        dr->host,
        path,
        post_data
    );

    free(path);
    free(post_data);

    if (NULL == response.data) {
        dump_response("Unable to parse HTTP response", &response);
        goto cleanup;
    }
    json_start = strstr(response.data, "{");
    if (NULL == json_start) {
        dump_response("No json response from server", &response);
        goto cleanup;
    }
    if (json_start != response.data) {
        dump_response("WARN: Expected JSON to start immediately in the returned data", &response);
    }

    ret = iotcl_discovery_parse_sync_response(json_start);
    if (!ret || ret->ds != IOTCL_SR_OK) {
        report_sync_error(ret, response.data);
        iotcl_discovery_free_sync_response(ret);
        ret = NULL;
    }

    cleanup:
    iotconnect_free_https_response(&response);
    // fall through

    return ret;
}

static void on_mqtt_c2d_message(const char* message) {
    Log.infof("event>>> %s", message);
    if (!iotcl_process_event(message)) {
        Log.error(F("Error encountered while processing the message"));
    }
}

void iotconnect_sdk_disconnect(void) {
    iotc_mqtt_client_disconnect();
    Log.info(F("Disconnected."));
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
            Log.info(F("Got ON_FORCE_SYNC. Disconnecting."));
            iotconnect_sdk_disconnect(); // client will get notification that we disconnected and will reinit

        case ON_CLOSE:
            Log.info(F("Got a disconnect request. Closing the mqtt connection. Device restart is required."));
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

///////////////////////////////////////////////////////////////////////////////////
// this the Initialization os IoTConnect SDK
bool iotconnect_sdk_init(void) {
    if (!config.cpid || !config.env || !config.duid) {
        Log.error(F("CPID, Environment and DUID are required for iotconnect_sdk_init()"));
        return false;
    }
    if (mqtt_config.sr) {
        iotcl_discovery_free_sync_response(mqtt_config.sr);
        mqtt_config.sr = NULL;
    }

    IotclDiscoveryResponse *discovery_response = run_http_discovery(config.cpid, config.env);
    if (NULL == discovery_response) {
        // run_http_discovery will print the error
        return false;
    }
    IotclSyncResponse *sr = run_http_sync(discovery_response, config.cpid, config.duid);
    iotcl_discovery_free_discovery_response(discovery_response); // we no longer need it
    if (NULL == sr) {
        // run_http_sync will print the error
        return false;
    }
    mqtt_config.sr = sr;
    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;

    iotc_get_time_modem();

    if (!config.env || !config.cpid || !config.duid) {
        Log.error(F("Error: Device configuration is invalid. Configuration values for env, cpid and duid are required."));
        return false;
    }

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = on_message_intercept;

    lib_config.telemetry.dtg = sr->dtg;

    char cpid_buff[5];
    strncpy(cpid_buff, config.cpid, 4);
    cpid_buff[4] = 0;
    Log.infof("CPID: %s***\r\n", cpid_buff);
    Log.infof("ENV:  %s\r\n", config.env);

    if (!iotcl_init(&lib_config)) {
        Log.error(F("Error: Failed to initialize the IoTConnect Lib"));
        return false;
    }

    mqtt_config.status_cb = config.status_cb;
    mqtt_config.c2d_msg_cb = on_mqtt_c2d_message;
    if (!iotc_mqtt_client_init(&mqtt_config)) {
        Log.error(F("Failed to connect!"));
        return false;
    }

    return true;
}
