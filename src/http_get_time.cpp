/**
 * @brief This example demonstrates how to retrieve the unix time from a NTP
 * server.
 */

#include <time.h>
#include <string.h>
#include <Arduino.h>
#include <log.h>
#include <lte.h>
#include <http_client.h>

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
        Log.errorf("http_get_time: Failed to configure HTTP for the domain %s. Is the network up?\r\n",
                   TIMEZONE_URL);
        return 0;
    }
    HttpResponse response = HttpClient.get(TIMEZONE_URI);
    if (response.status_code != HttpClient.STATUS_OK) {
        Log.errorf("http_get_time: GET request on %s%s failed. Got status code %d\r\n",
                   TIMEZONE_URL,
                   TIMEZONE_URI,
                   response.status_code);
        return 0;
    }

/*
    Log.infof(
        "Successfully performed GET request. Status Code = %d, Size = %d\r\n",
        response.status_code,
        response.data_size);
*/
    String body = HttpClient.readBody(512);

    if (body == "") {
        Log.error("http_get_time:  The returned body from the GET request is empty!");
        return 0;
    }
    time_t now = parse_time_from_response(&body);
    if (0 == now) {
        Log.error("http_get_time: Unable to process the time response!");
        return 0;
    }
    rtc_start = now - (time_t)(millis()/1000);
    return now;
}

//override time() system function
time_t time(time_t *tloc) {
    Log.info("Time called");
  return(rtc_start + (time_t)(millis()/1000));
}

time_t http_get_time(int max_tries) {
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
