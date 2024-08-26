/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#include <Arduino.h>
#include "log.h"
#include "iotcl_arduino_log_cfg.h"

#define IOTCL_LOG_ENDLN (F("\r\n"))
#define IOTCL_LOG_HDR_FMT (F("IOTCL ERROR [%d]: "))


void iotcl_arduino_log(int level, int err_code, const __FlashStringHelper* format, ...) {
    (void) err_code;
    va_list args;
    va_start(args, format);
    switch (level) {
        case IOTCL_LEVEL_ERROR:
            Log.errorf(F("IOTCL ERROR [%d]: "), level);
            Log.errorf(format, args);
            Log.errorf(F("\r\n"));
            break;
        case IOTCL_LEVEL_WARN:
            Log.warnf(F("IOTCL WARN [%d]: "), level);
            Log.warnf(format, args);
            Log.warnf(F("\r\n"));
            break;
        case IOTCL_LEVEL_INFO:
            Log.infof(F("IOTCL: "));
            Log.errorf(format, args);
            Log.infof(F("\r\n"));
            break;
        default:
            break;
    }
    va_end(args);
 }