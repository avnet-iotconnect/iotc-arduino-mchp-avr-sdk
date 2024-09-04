/* SPDX-License-Identifier: MIT
 * Copyright (C) 2020 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#include <time.h>
#include <string.h>
#include <Arduino.h>
#include "log.h"
#include "lte.h"
#include "http_client.h"
#include "sequans_controller.h"

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt" // This URI does not really matter. We are looking at UTC time

time_t rtc_start = 0;

static time_t parse_time_from_response(String* resp) {
    static const char* UNIXTIME_FIELD = "unixtime: ";
    int unix_time_index    = resp->indexOf(String(UNIXTIME_FIELD));
    int comma_index = resp->indexOf(String(","));


    String time_str = resp->substring(unix_time_index + strlen(UNIXTIME_FIELD), comma_index);
    char * endptr = NULL;
    unsigned long ret = strtoul(time_str.c_str(), &endptr, 10);
    if (endptr == NULL) {
        return 0;
    }
    return(time_t) ret;
}

static time_t do_http_get_time(void) {
    if (!HttpClient.configure(TIMEZONE_URL, 80, false)) {
        Log.errorf(F("http_get_time: Failed to configure HTTP for the domain %s. Is the network up?\n"),
                   TIMEZONE_URL);
        return 0;
    }
    HttpResponse response = HttpClient.get(TIMEZONE_URI);
    if (response.status_code != HttpClient.STATUS_OK) {
        Log.errorf(F("http_get_time: GET request on %s%s failed. Got status code %d\n"),
                   TIMEZONE_URL,
                   TIMEZONE_URI,
                   response.status_code);
        return 0;
    }

#if 0
    Log.infof(
        "Successfully performed GET request. Status Code = %d, Size = %d\n",
        response.status_code,
        response.data_size);
#endif
    String body = HttpClient.readBody(512);

    if (body == "") {
        Log.error(F("http_get_time:  The returned body from the GET request is empty!"));
        return 0;
    }
    time_t now = parse_time_from_response(&body);
    if (0 == now) {
        Log.error(F("http_get_time: Unable to process the time response!"));
        return 0;
    }
    rtc_start = now - (time_t)(millis()/1000);
    return now;
}

//override time() system function
time_t time(time_t *tloc) {
  return(rtc_start + (time_t)(millis()/1000));
}

time_t iotc_get_time_http(int max_tries) {
    if (max_tries < 1) {
        max_tries = 0; // silly, but possible
    }
    for (int i = 0; i < max_tries; i++) {
        time_t ret = do_http_get_time();
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

static time_t cclk_response_to_time_t(const char* time_str)
{
    time_t now_time_t;
    int hh, mm, ss, yy, mon, day, tzo;
    char tz_sign;
    struct tm now_tm = {0};

    int num = sscanf(time_str, "\n+CCLK: \"%d/%d/%d,%d:%d:%d%c%d\"\n", &yy, &mon, &day, &hh, &mm, &ss, &tz_sign, &tzo);
    if (num != 8)  { // need to match 8 items &yy, &mon, &day etc.
        Log.errorf(F("Unable to parse modem time %s"), time_str);
        return 0;
    }

    if (70 == yy) {
        Log.warn(F("Modem time is not ready"));
        return 0; // I guess this could be 1970. Modem seems to report "70/01/01,00:02:21+00" when offline
    }
    now_tm.tm_isdst = -1;
    now_tm.tm_hour = hh;
    now_tm.tm_min = mm;
    now_tm.tm_sec = ss;
    now_tm.tm_year = 2000 + yy - 1900;
    now_tm.tm_mon = mon - 1;
    now_tm.tm_mday = day;
    now_time_t = mktime(&now_tm);

    Log.infof(F("+CCLK: \"%d/%d/%d,%d:%d:%d%c%d\"\n"), yy, mon, day, hh, mm, ss, tz_sign, tzo);

    // tz offset is quarter of an hour from gmt. 15 minutes have 900 seconds
    // To get GMT time, we need to subtract (not add) the signed offset
    int sign = tz_sign == '-' ? -1 : 1;
    now_time_t -= sign * 900 * tzo;
    return now_time_t;
}

time_t iotc_get_time_modem(void) {
    // If low power is utilized, sequans controller will already been
    // initialized, so don't reset it by calling begin again
    if (!SequansController.isInitialized()) {
        SequansController.begin();
    }

    char response_buffer[64] = "";
    char value_buffer[32] = "";

    ResponseResult res = SequansController.writeCommand( "AT+CCLK?", response_buffer, sizeof(response_buffer));
    if (res != ResponseResult::OK) {
        Log.error(F("Failed to retrieve successful response time from the modem"));
        return 0;
    }

    if (!SequansController.extractValueFromCommandResponse(
        response_buffer,
        0,
        value_buffer,
        sizeof(value_buffer))) {
            Log.error(F("Failed to retrieve time from the modem"));
            return 0;
    }
    Log.debugf(F("Time AT response: >>%s<<\n"), response_buffer);

    time_t now = cclk_response_to_time_t(response_buffer);
    if (0 == now) {
        return 0; // the called function will report the error
    }

    rtc_start = now - (time_t)(millis()/1000);

    return now;
}