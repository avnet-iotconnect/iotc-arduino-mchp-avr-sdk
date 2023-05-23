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
ATCA_STATUS iotc_ecc608_read_string_value(ecc_data_types data_type, char ** value);
// Write the value pointed by "value" to ecc608 data storage
ATCA_STATUS iotc_ecc608_write_string_value(ecc_data_types data_type, const char * value);


