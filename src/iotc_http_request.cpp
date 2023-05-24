//
// Copyright: Avnet 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 4/5/23.
//

#include <stdlib.h>
#include <string.h>
#include "http_client.h"
#include "log.h"
#include "iotc_http_request.h"

int iotconnect_https_request(
        IotConnectHttpResponse *response,
        const char *host,
        const char *path,
        const char *send_str
) {
    response->data = NULL;
    if (!HttpClient.configure(host, 443, true)) {
        Log.error("Failed to configure https client");
        return -1;
    }

    HttpResponse http_rsp;
    if (!send_str || 0 == strlen(send_str)) {
        Log.debugf("get: %s %s\r\n", host, path);
        http_rsp = HttpClient.get(path);
    } else {
        Log.debugf("post: %s %s >>%s<<\r\n", host, path, send_str);
        http_rsp = HttpClient.post(
            path,
            send_str,
            NULL,
            HttpClientClass::CONTENT_TYPE_APPLICATION_JSON,
            HTTP_DEFAULT_TIMEOUT_MS
        );
    }

    if (0 == http_rsp.status_code) {
        Log.errorf("Unable to get response from the server for URL https://%s%s\r\n", host, path);
        return -1;
    }

    if (200 != http_rsp.status_code) {
        Log.warnf("Unexpected HTTP response status code %u\r\n", http_rsp.status_code);
    }
    // Responses should not be
    String body = HttpClient.readBody(1000);
    size_t body_length = body.length();
    if (0 == body_length) {
        Log.error("Http response was empty");
        return -1;
    }
    response->data = (char *) malloc(body_length + 1);
    strcpy(response->data, body.c_str());
    return 0;
}


void iotconnect_free_https_response(IotConnectHttpResponse *response) {
    if (response->data) {
        free(response->data);
    }
    response->data = NULL;
}