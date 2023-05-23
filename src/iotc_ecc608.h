#ifndef IOTC_ECC608_H
#define IOTC_ECC608_H

//
// Copyright: Avnet 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 5/16/21.
//

#include "ecc608.h"
#include "cryptoauthlib/app/tng/tng_atcacert_client.h"

#define IOTC_ECC608_PROV_CPID GOOGLE_PROJECT_ID
#define IOTC_ECC608_PROV_ENV  GOOGLE_PROJECT_REGION
#define IOTC_ECC608_PROV_VER  GOOGLE_REGISTRY_ID      // Data storage version (check)
#define IOTC_ECC608_PROV_DUID GOOGLE_DEVICE_ID

ATCA_STATUS iotc_ecc608_init_provision(void);

void iotc_ecc608_dump_provision_data(void);

// Read data_type value into storage pointed by the "value"
ATCA_STATUS iotc_ecc608_get_string_value(ecc_data_types data_type, char ** value);

// Read the value into "buffer".
ATCA_STATUS iotc_ecc608_copy_string_value(ecc_data_types data_type, char *buffer, size_t buffer_size);

// Write data to local cache, but to not write. Call commit() in order to write data into ecc608.
ATCA_STATUS iotc_ecc608_set_string_value(ecc_data_types data_type, const char * value);

// Write data to ecc608 Data Zone Slot 8
ATCA_STATUS iotc_ecc608_write_all_data(void);

#endif // IOTC_ECC608_H