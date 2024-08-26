/* SPDX-License-Identifier: MIT
 * Copyright (C) 2024 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */
#ifndef IOTCL_ARDUINO_LOG_CFG_H
#define IOTCL_ARDUINO_LOG_CFG_H

#include <Arduino.h>
#include "log.h"

#ifndef IOTCL_ENDLN
#define IOTCL_ENDLN "\r\n"
#endif

#define IOTCL_LEVEL_ERROR 0
#define IOTCL_LEVEL_WARN  1
#define IOTCL_LEVEL_INFO  2


void iotcl_arduino_log(int level, int err_code, const __FlashStringHelper* format, ...);

#define IOTCL_ERROR(err_code, fmt, ...) \
    do { \
        iotcl_arduino_log(IOTCL_LEVEL_ERROR, err_code, F(fmt), ## __VA_ARGS__); \
    } while(0)

#define IOTCL_WARN(err_code, fmt, ...) \
    do { \
        iotcl_arduino_log(IOTCL_LEVEL_WARN, err_code, F(fmt), ## __VA_ARGS__); \
    } while(0)

#define IOTCL_INFO(fmt, ...) \
    do { \
        iotcl_arduino_log(IOTCL_LEVEL_INFO, 0, F(fmt), ## __VA_ARGS__); \
    } while(0)



#endif // IOTCL_CFG_H