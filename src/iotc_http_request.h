//
// Copyright: Avnet 2021
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#ifndef IOTC_HTTP_REQUEST_H
#define IOTC_HTTP_REQUEST_H

typedef struct IotConnectHttpResponse {
    char *data; // add flexibility for future, but at this point we only have response data
} IotConnectHttpResponse;

// Helper to deal with http chunked transfers which are always returned by iotconnect services.
// Free data with iotconnect_free_https_response
int iotconnect_https_request(
        IotConnectHttpResponse* response,
        const char *host,
        const char *path,
        const char *send_str
);

void iotconnect_free_https_response(IotConnectHttpResponse* response);


#endif // IOTC_DISCOVERY_CLIENT_H