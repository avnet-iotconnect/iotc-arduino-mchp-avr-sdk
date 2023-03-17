/**
 * @brief This example demonstrates how to retrieve the unix time from a NTP
 * server.
 */

#include <time.h>
#include <Arduino.h>
#include <http_client.h>
#include <log.h>

#define TIMEZONE_URL "worldtimeapi.org"
#define TIMEZONE_URI "/api/timezone/Europe/Oslo.txt" // This URI does not really matter. We are looking at UTC time

time_t getTimeFromResponse(String* resp) {
    int unix_time_index    = resp->indexOf(String("unixtime: "));
    int utx_datetime_index = resp->indexOf(String("utc_datetime"));

    return (time_t)resp->substring(unix_time_index + 10, utx_datetime_index - 1)
        .toInt();
}

time_t http_get_time(void) {
    if (!HttpClient.configure(TIMEZONE_URL, 80, false)) {
        Log.errorf("Failed to configure HTTP for the domain %s\r\n",
                   TIMEZONE_URL);
        return 0;
    }

    HttpResponse response;
    response = HttpClient.get(TIMEZONE_URI);
    if (response.status_code != HttpClient.STATUS_OK) {
        Log.errorf("Error when performing a GET request on %s%s. Got status "
                   "code = %d. Exiting...\r\n",
                   TIMEZONE_URL,
                   TIMEZONE_URI,
                   response.status_code);
        return 0;
    }

    Log.infof(
        "Successfully performed GET request. Status Code = %d, Size = %d\r\n",
        response.status_code,
        response.data_size);

    String body = HttpClient.readBody(512);

    if (body == "") {
        Log.errorf("The returned body from the GET request is empty. Something "
                   "went wrong. Exiting...\r\n");
        return 0;
    }

    return getTimeFromResponse(&body);
}
