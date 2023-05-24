#ifndef IOTC_ECC608_H
#define IOTC_ECC608_H

/*
  (c) 2022 Microchip Technology Inc. and its subsidiaries.

  Subject to your compliance with these terms, you may use Microchip software
  and any derivatives exclusively with Microchip products. You're responsible
  for complying with 3rd party license terms applicable to your use of 3rd
  party software (including open source software) that may accompany Microchip
  software.

  SOFTWARE IS "AS IS." NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY,
  APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,
  MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.

  IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
  INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
  WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
  HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
  THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
  CLAIMS RELATED TO THE SOFTWARE WILL NOT EXCEED AMOUNT OF FEES, IF ANY,
  YOU PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*/


//
// Copyright: Avnet 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 5/16/23.
//

#include "ecc608.h"
#include "cryptoauthlib/app/tng/tng_atcacert_client.h"

#define IOTC_ECC608_PROV_CPID GOOGLE_PROJECT_ID
#define IOTC_ECC608_PROV_ENV  GOOGLE_PROJECT_REGION
#define IOTC_ECC608_PROV_VER  GOOGLE_REGISTRY_ID      // Data storage version (check)
#define IOTC_ECC608_PROV_DUID GOOGLE_DEVICE_ID

// Some sizes including null:
#define IOTC_ECC608_PROV_DUID_SIZE 66
#define IOTC_ECC608_PROV_CPID_SIZE 66
#define IOTC_ECC608_PROV_ENV_SIZE 20


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