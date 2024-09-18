/* SPDX-License-Identifier: MIT
 * Copyright (C) 2020 Avnet
 * Authors: Nikola Markovic <nikola.markovic@avnet.com> et al.
 */

#ifndef IOTC_TIME_H
#define IOTC_TIME_H

#include <time.h>

time_t iotc_get_time_http(int max_tries = 5);

time_t iotc_get_time_modem(void);

#endif // IOTC_TIME_H