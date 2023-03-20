/**
 * @brief This example demonstrates how to retrieve the unix time from a NTP
 * server.
 */

#include <time.h>
#include <Arduino.h>
#include <log.h>
#include <lte.h>
#include <http_client.h>

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt" // This URI does not really matter. We are looking at UTC time

time_t rtc_start = 0;

static time_t parse_time_from_response(String* resp) {
    int unix_time_index    = resp->indexOf(String("unixtime: "));
    int utx_datetime_index = resp->indexOf(String("utc_datetime"));

    return (time_t)resp->substring(unix_time_index + strlen("unixtime: "), utx_datetime_index - 1)
        .toInt();
}

static time_t do_http_get_time(void) {
    if (!HttpClient.configure(TIMEZONE_URL, 80, false)) {
        Log.errorf("http_get_time: Failed to configure HTTP for the domain %s. Is the network up?\r\n",
                   TIMEZONE_URL);
        return 0;
    }

    HttpResponse response;
    response = HttpClient.get(TIMEZONE_URI);
    if (response.status_code != HttpClient.STATUS_OK) {
        Log.errorf("http_get_time: Error when performing a GET request on %s%s. Got status "
                   "code = %d. Exiting...\r\n",
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
        Log.errorf("The returned body from the GET request is empty. Something "
                   "went wrong. Exiting...\r\n");
        return 0;
    }
    time_t now = parse_time_from_response(&body);
    Log.infof("DEBUG: %lu", now);
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
