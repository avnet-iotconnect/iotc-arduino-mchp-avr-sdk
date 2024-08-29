/* SPDX-License-Identifier: MIT
 * Copyright (C) 2020 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#include <string.h>
#include <Arduino.h>
#include "log.h"

#include "iotcl.h"
#include "iotcl_util.h"
#include "iotcl_dra_discovery.h"
#include "iotcl_dra_identity.h"
#include "iotc_time.h"
#include "iotc_http_request.h"
#include "iotc_mqtt_client.h"
#include "IoTConnectSDK.h"

static bool is_verbose = false;
static IotConnectMqttClientConfig mqtt_config = {0};

static void dump_response(const char *message, IotConnectHttpResponse *response) {
    if (message) {
        Log.infof(F("%s:\n"), message);
    }
    if (response->data) {
        Log.infof(F("Response was:\n----\n%s\r\n----\r\n"), response->data);
    } else {
        Log.infof(F("Response was empty\r\n"));
    }
}

static int validate_response(IotConnectHttpResponse *response) {
    if (NULL == response->data) {
        dump_response("Unable to parse HTTP response.", response);
        return IOTCL_ERR_PARSING_ERROR;
    }
    const char *json_start = strstr(response->data, "{");
    if (NULL == json_start) {
        dump_response("No json response from server.", response);
        return IOTCL_ERR_PARSING_ERROR;
    }
    if (json_start != response->data) {
        dump_response("WARN: Expected JSON to start immediately in the returned data.", response);
    }
    return IOTCL_SUCCESS;
}

static int run_http_identity(IotConnectConnectionType ct, const char* duid, const char *cpid, const char *env) {
    IotConnectHttpResponse response = {0};
    IotclDraUrlContext discovery_url = {0};
    IotclDraUrlContext identity_url = {0};
    int status;
    switch (ct) {
        case IOTC_CT_AWS:
        	// TEMPORARY HACK UNTIL WE SORT OUT DISCOVERY URL
        	// Shortcut to avoid full case insensitive match.
        	// Hopefully the user doesn't enter something with mixed case
        	if (0 == strcmp("POC", env) || 0 == strcmp("poc", env)) {
                status = iotcl_dra_discovery_init_url_with_host(&discovery_url, (char *) "awsdiscovery.iotconnect.io", cpid, env);
        	} else {
                status = iotcl_dra_discovery_init_url_with_host(&discovery_url, (char *) "discoveryconsole.iotconnect.io", cpid, env);
        	}
        	if (IOTCL_SUCCESS == status) {
            	Log.infof(F("Using AWS discovery URL %s\n"), iotcl_dra_url_get_url(&discovery_url));
        	}
            break;
        case IOTC_CT_AZURE:
            status = iotcl_dra_discovery_init_url_azure(&discovery_url, cpid, env);
        	if (IOTCL_SUCCESS == status) {
            	Log.infof(F("Using Azure discovery URL %s\n"), iotcl_dra_url_get_url(&discovery_url));
        	}
            break;
        default:
            Log.infof(F("Unknown connection type %d\n"), ct);
            return IOTCL_ERR_BAD_VALUE;
    }

    if (status) {
        return status; // called function will print the error
    }

    if (iotconnect_https_request(&response,
                             iotcl_dra_url_get_hostname(&discovery_url),
							 iotcl_dra_url_get_resource(&discovery_url),
							 NULL
    )) {
        goto cleanup;
    }

    status = validate_response(&response);
    if (status) goto cleanup; // called function will print the error


    status = iotcl_dra_discovery_parse(&identity_url, 0, response.data);
    if (status) {
        Log.errorf(F("Error while parsing discovery response from %s\n"), iotcl_dra_url_get_url(&discovery_url));
        dump_response(NULL, &response);
        goto cleanup;
    }

    iotconnect_free_https_response(&response);
    memset(&response, 0, sizeof(response));

    status = iotcl_dra_identity_build_url(&identity_url, duid);
    if (status) goto cleanup; // called function will print the error

    if(iotconnect_https_request(&response,
                             iotcl_dra_url_get_hostname(&identity_url),
							 iotcl_dra_url_get_resource(&identity_url),
							 NULL
    )) {
        goto cleanup;
    }

    status = validate_response(&response);
    if (status) goto cleanup; // called function will print the error

    status = iotcl_dra_identity_configure_library_mqtt(response.data);
    if (status) {
        Log.errorf(F("Error while parsing identity response from %s\n"), iotcl_dra_url_get_url(&identity_url));
        dump_response(NULL, &response);
        goto cleanup;
    }

    if (ct == IOTC_CT_AWS && iotcl_mqtt_get_config()->username) {
        // workaround for identity returning username for AWS.
        // https://awspoc.iotconnect.io/support-info/2024036163515369
        iotcl_free(iotcl_mqtt_get_config()->username);
        iotcl_mqtt_get_config()->username = NULL;
    }

    cleanup:
    iotcl_dra_url_deinit(&discovery_url);
    iotcl_dra_url_deinit(&identity_url);
    iotconnect_free_https_response(&response);
    return status;
}

static void iotconnect_sdk_mqtt_send_cb(const char *topic, const char *json_str) {
    if (is_verbose) {
        Log.infof(F(">: %s\n"), json_str);
    }
    iotc_mqtt_client_send_message(topic, json_str);
}


static void on_mqtt_message(const char* message) {
    if (is_verbose) {
        Log.infof(F("event>>> %s"), message);
    }
    iotcl_c2d_process_event(message);
}

void iotconnect_sdk_disconnect(void) {
    iotc_mqtt_client_disconnect();
    Log.info(F("Disconnected."));
}

bool iotconnect_sdk_is_connected(void) {
    return iotc_mqtt_client_is_connected();
}


void iotconnect_sdk_loop(void) {
    return iotc_mqtt_client_loop();
}

///////////////////////////////////////////////////////////////////////////////////
// this the Initialization os IoTConnect SDK
bool iotconnect_sdk_init(IotConnectClientConfig *c) {
    int status;

    if (!c->cpid || !c->env || !c->duid) {
        Log.error(F("CPID, Environment and DUID are required for iotconnect_sdk_init()"));
        return false;
    }

    is_verbose = c->verbose;

    IotclClientConfig iotcl_cfg;
    iotcl_init_client_config(&iotcl_cfg);
    iotcl_cfg.device.cpid = c->cpid;
    iotcl_cfg.device.duid = c->duid;
    iotcl_cfg.device.instance_type = IOTCL_DCT_CUSTOM; //we will use discovery, so CUSTOM
    iotcl_cfg.mqtt_send_cb = iotconnect_sdk_mqtt_send_cb;
    iotcl_cfg.events.cmd_cb = c->cmd_cb;
    iotcl_cfg.events.ota_cb = c->ota_cb;

    iotc_get_time_modem();

    if (c->verbose) {
        status = iotcl_init_and_print_config(&iotcl_cfg);
    } else {
        status = iotcl_init(&iotcl_cfg);
    }
    if (status) {
        // called function will print the error
        return false;
    }

	status = run_http_identity(c->connection_type, c->duid, c->cpid, c->env);
    if (status) {
		iotcl_deinit();
        return false;
    }
    Log.info(F("Identity response parsing successful."));

    mqtt_config.status_cb = c->status_cb;
    mqtt_config.c2d_msg_cb = on_mqtt_message;
    if (!iotc_mqtt_client_init(&mqtt_config)) {
        Log.error(F("Failed to connect!"));
        return false;
    }

    return true;
}
