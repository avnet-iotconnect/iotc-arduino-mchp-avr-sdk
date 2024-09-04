/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */
#ifndef IOTCL_ARDUINO_LOG_CFG_H
#define IOTCL_ARDUINO_LOG_CFG_H

#include <Arduino.h>
#include "log.h"

#ifdef IOTCL_ENDLN
#undef IOTCL_ENDLN
#endif
#define IOTCL_ENDLN F("\n")

#define IOTCL_ERROR(err_code, fmt, ...) \
    do { \
            Log.errorf(F("IOTCL [%d]: "), err_code); \
            Log.rawf(F(fmt), ## __VA_ARGS__); \
            Log.rawf(IOTCL_ENDLN); \
    } while(0)

#define IOTCL_WARN(err_code, fmt, ...) \
    do { \
            Log.warnf(F("IOTCL [%d]: "), err_code); \
            Log.rawf(F(fmt), ## __VA_ARGS__); \
            Log.rawf(IOTCL_ENDLN); \
    } while(0)

#define IOTCL_INFO(fmt, ...) \
    do { \
            Log.infof(F("IOTCL: ")); \
            Log.rawf(F(fmt), ## __VA_ARGS__); \
            Log.rawf(IOTCL_ENDLN); \
    } while(0)



#endif // IOTCL_CFG_H