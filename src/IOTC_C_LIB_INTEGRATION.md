# About

This document is provided to describe th steps needed to integrate the iotc-c-lib into the project
ino order to assist with future integration of iotc-c-lib updates.

# Integration Steps

### Drop files directly into src
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
