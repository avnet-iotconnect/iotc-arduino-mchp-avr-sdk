# About

This internal document is provided to describe th steps needed to integrate the iotc-c-lib and make other changes 
to the existing files to make them work in the Arduino AVR environment.

# AVR Library Changes

iotc_http_request.cpp has an added function fixed_http_client_read_body(), which is 
a patched version http_request.c HttpClient::readBody so that it handles SQNHTTPRCV properly when returning ERROR 
when there is no data from the sever. If ERROR is received, -2 will be retruned and the main fetch loop should break.

There are some buffer and fetch size related changes to accommodate for smaller chunks.


# C-Lib Integration Steps

### Drop c-lib files directly into src
Since the Arduino environment is not flexible enough, we have to drop raw iotc-c-lib files from 
the core and device-rest api module into the SDK directly. 

### Logging hack
As of the time of writing, the Arduino IDE does not allow passing compile defines so that we could override the
logging macros properly, so we have to hack the library code in a few places, so locate this snippet in iotcl_log.h:

```C
// MBEDTLS config file style - include your own to override the config. See iotcl_example_config.h
#if defined(IOTCL_USER_CONFIG_FILE)
#include IOTCL_USER_CONFIG_FILE
#endif
```

and append this code:

```C
// iotc-c-lib hack for Arduino:
#include "iotcl_arduino_config.h"
```

### The F() macro problem workaround

The F() macro is used to tell the arduino compiler that the constant strings wrapped around it 
should be used directly from flash. If the F() macro is not used for constant strings (often used for printf and similar)
then the strings in flash are copied to RAM and used from RAM.

### Debugging the modem:

If modem debvugging is needed, the following code can be added to sequans_controlller.cpp

```C
        if (data == CARRIAGE_RETURN || data == LINE_FEED) {

            // Add termination since we're done
            urc_data_buffer[urc_data_buffer_length] = 0;
            // Clear the buffer for the URC if requested and if it already
            // hasn't been read
            if (urcs[urc_index].should_clear &&
                rx_num_elements >= urc_data_buffer_length) {

                rx_head_index = (rx_head_index - urc_data_buffer_length) &
                                RX_BUFFER_MASK;
                rx_num_elements = (rx_num_elements - urc_data_buffer_length);
            }

            if (urc_current_callback != NULL) {
                // Apply flow control here for the modem, we make it wait to
                // send more data until we've finished the URC callback
                RTS_PORT.OUTSET = RTS_PIN_bm;
                urc_current_callback((char*)urc_data_buffer);
                urc_current_callback = NULL;
                RTS_PORT.OUTCLR      = RTS_PIN_bm;
            }
#if 0
            Serial3.print("#");
            Serial3.print((char*)urc_identifier_buffer);
            Serial3.print("=");
            Serial3.print((char*)urc_data_buffer);
            Serial3.print("<");
            Serial3.println();
#endif
            urc_parse_state        = URC_NOT_PARSING;
            urc_data_buffer_length = 0;

        } else if (urc_data_buffer_length == URC_DATA_BUFFER_SIZE) {

```