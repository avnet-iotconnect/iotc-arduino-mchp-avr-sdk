/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#include <util/delay.h>
#include <stdlib.h>
#include <string.h>

#include "sequans_controller.h"

#include "iotcl.h"
#include "iotcl_cfg.h"
#include "http_client.h"
#include "log.h"
#include "iotc_http_request.h"

// rough number of extra buffer bytes we need to not cause overflow when 
// requesting a number of bytes from the http response
// This slack will also account for an extra trailing null
#define HTTP_RESPONSE_BUFFER_SLACK 10

// BEGIN HttpClientClass::readBody fix
// The HttpClientClass::readBody function does not account for extra buffer space needed 
// for the response data from the modem.
// We also return -1 if we get an error from readResponse
// I think that we need this so that we can know when to stop and differentiate versus an error
// See http_client.cpp

// re-defined:
#define HTTP_BODY_BUFFER_MIN_SIZE (64)
#define HTTP_BODY_BUFFER_MAX_SIZE (1800)

static int16_t fixed_http_client_read_body(char* buffer, const uint32_t buffer_size, const uint32_t request_bytes) {

    // Safeguard against the limitation in the Sequans AT command parameter
    // for the response receive command.
    if (buffer_size < HTTP_BODY_BUFFER_MIN_SIZE) {
        Log.errorf(F("Buffer should have at least %d bytes\n"), HTTP_BODY_BUFFER_MIN_SIZE);
        return -1;
    }

    if (buffer_size > HTTP_BODY_BUFFER_MAX_SIZE) {
        Log.errorf(F("Buffer should have at most %d bytes\n"), HTTP_BODY_BUFFER_MAX_SIZE);
        return -1;
    }

    if (buffer_size < request_bytes + HTTP_RESPONSE_BUFFER_SLACK) {
        Log.errorf(F("Buffer size must have at least %d extra bytes to accomodate the request\n"), HTTP_RESPONSE_BUFFER_SLACK);
        return -1;
    }

    // Fix for bringing the modem out of idling and prevent timeout whilst
    // waiting for modem response during the next AT command
    SequansController.writeCommand(F("AT"));

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    if (!SequansController.writeString(F("AT+SQNHTTPRCV=0,%lu"),
                                       true,
                                       request_bytes)) {
        Log.error(F("Was not able to write HTTP read body AT command\n"));
        return -1;
    }

    // We receive three start bytes '<', have to wait for them
    uint8_t start_bytes = 3;
    while (start_bytes > 0) {
        if (SequansController.waitForByte('<', 1000)) {
            start_bytes--;
        } else {
            return -2; // timed out
        }
    }

    // Now we are ready to receive the payload. We only check for error and
    // not overflow in the receive buffer in comparison to our buffer as we
    // know the size of what we want to receive
    ResponseResult res = SequansController.readResponse(buffer, buffer_size);
    if (res !=
        ResponseResult::OK) {
        Log.debugf(F("readResponse result %d\n"), (int) res);
        return -1;
    }

    return strlen(buffer);
}

int iotconnect_https_request(
        IotConnectHttpResponse *response,
        const char *host,
        const char *path,
        const char *send_str
) {
    response->data = NULL;
    if (!HttpClient.configure(host, 443, true)) {
        Log.error(F("Failed to configure https client"));
        return IOTCL_ERR_FAILED;
    }

    HttpResponse http_rsp;
    if (!send_str || 0 == strlen(send_str)) {
        Log.debugf(F("get: %s %s\n"), host, path);
        http_rsp = HttpClient.get(path);
    } else {
        Log.debugf(F("post: %s %s >>%s<<\n"), host, path, send_str);
        http_rsp = HttpClient.post(
            path,
            send_str,
            NULL,
            HttpClientClass::CONTENT_TYPE_APPLICATION_JSON,
            HTTP_DEFAULT_TIMEOUT_MS
        );
    }

    if (0 == http_rsp.status_code) {
        Log.errorf(F("Unable to get response from the server for URL https://%s%s\n"), host, path);
        return IOTCL_ERR_FAILED;
    }

    if (200 != http_rsp.status_code) {
        Log.warnf(F("Unexpected HTTP response status code %u\n"), http_rsp.status_code);
    }

    Log.debugf(F("Reported data size is %u\n"), http_rsp.data_size);

    const size_t BUFFER_SIZE = 2000;
    // we need to allow slack for the buffer
    const size_t REQUEST_DATA_MAX_SIZE = BUFFER_SIZE - HTTP_BODY_BUFFER_MIN_SIZE;

    if (!http_rsp.data_size) {
        // we didn't get content length, so must be chunked transfer
        Log.debugf(F("HTTP Client: Did not get content-length. Getting up to %d bytes of data\n"), REQUEST_DATA_MAX_SIZE);
    } else if (http_rsp.data_size > REQUEST_DATA_MAX_SIZE) {
        Log.errorf(F("HTTP Client: Content length is %d but the buffer can accomodate up to %d bytes of data\n"), (int) http_rsp.data_size, REQUEST_DATA_MAX_SIZE);
        return IOTCL_ERR_OVERFLOW;
    }

    response->data = (char *) iotcl_malloc(BUFFER_SIZE);
    if (!response->data ) {
        Log.errorf(F("HTTP Client: Failed to allocate %d bytes!\n"), (int) BUFFER_SIZE);
        return IOTCL_ERR_OUT_OF_MEMORY;
    }
    memset(response->data, 0, BUFFER_SIZE);

    // do roughly 1k at a time as there could be other limitations on the modem side
    const size_t CHUNK_SIZE = REQUEST_DATA_MAX_SIZE / 2; 
    size_t total_read = 0;
    int16_t chunk_bytes_read = 0;
    int data_chunk_size = 0; // we need this value checked at the end of do/while
    do {
        // read up to CHUNK_SIZE bytes at a time...
        data_chunk_size = CHUNK_SIZE;
        // ...unless would we exceed the buffer by the next request
        if (total_read + CHUNK_SIZE > REQUEST_DATA_MAX_SIZE) {
            data_chunk_size = REQUEST_DATA_MAX_SIZE - total_read;
        }

        size_t buffer_remaining = BUFFER_SIZE - total_read;
        if (buffer_remaining > HTTP_BODY_BUFFER_MAX_SIZE) {
            // we need to not hit the error condition where buffer is too 
            // just use a truncated buffer range unless we reach the boundary
            buffer_remaining = HTTP_BODY_BUFFER_MAX_SIZE - 1;
        } else if(buffer_remaining < HTTP_BODY_BUFFER_MIN_SIZE) {
            Log.error(F("HTTP Client: Response overflow. Ran out of buffer space!\n"));
            iotconnect_free_https_response(response);
            return IOTCL_ERR_OVERFLOW;  
        }
        chunk_bytes_read = fixed_http_client_read_body(&response->data[total_read], buffer_remaining, data_chunk_size);
        if (chunk_bytes_read == -2) {
            // timed out waiting for data. That's all we got
            Log.debug(F("HTTP: No mode data."));
            break;
        } else if (chunk_bytes_read == 0) {
            break;
        } else if (chunk_bytes_read > 0) {
            total_read += (size_t) chunk_bytes_read;
            Log.debugf(F("HTTP: Read %d bytes\n"), (int) chunk_bytes_read);
        } else {
            Log.debugf(F("HTTP: Error %d\n"), (int) chunk_bytes_read);
            iotconnect_free_https_response(response);
            return IOTCL_ERR_FAILED;
        }
        // we are done when we get the full chunk
    } while (chunk_bytes_read == data_chunk_size);

    if (0 == total_read) {
        Log.error(F("Http response was empty"));
        iotconnect_free_https_response(response);
        return IOTCL_ERR_FAILED;
    }
    response->data[total_read] = '\0'; // make sure that the srtring is terminated
    return IOTCL_SUCCESS;
}

void iotconnect_free_https_response(IotConnectHttpResponse *response) {
    if (response->data) {
        iotcl_free(response->data);
    }
    response->data = NULL;
}
