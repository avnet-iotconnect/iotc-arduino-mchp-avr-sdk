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
// Copyright: Avnet, Microchip Technology Inc 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 5/16/23.
//

#include "log.h"
#include "cryptoauthlib/app/tng/tng_atcacert_client.h"
#include "iotc_ecc608.h"

static ATCAIfaceCfg cfg_atecc608b_i2c = {
    ATCA_I2C_IFACE,
    ATECC608B,
    {
      0x58,  // 7 bit address of ECC
      2,     // Bus number
      100000 // Baud rate
    },
    1560,
    20,
    NULL
};


#define ECC608_MAX_DATA_ENTRY_SIZE 129 // What we think is the largest field...  1 for null
#define DATA_SLOT_NUM 8 // ECC slot number used for provisioning data

#ifndef IOTC_DATA_SLOT_SIZE
#define IOTC_DATA_SLOT_SIZE 416 // This should be the full size of the data zone slot 8
#endif

// How many bytes will each string have in reserved storaage.
// Storage content may be arbitrary after the first null terminator.

// For storage version 1.0, the storage was meant for azure, so just "v1.0"
// For 2.0 and onward we encode platofrm type into the version and we keep the same size,
// so 2azr means header version is 2 and platform is azure, and similar for AWS
// PV = platform and header version
#define IOTC_ECC608_PROV_DATA_PV_AZURE "2azr"
#define IOTC_ECC608_PROV_DATA_PV_AWS   "2aws"
#define IOTC_ECC608_PROV_DATA_PV_1_0   "v1.0" // will convert to IOTC_ECC608_PROV_DATA_PV_AZURE
#define IOTC_ECC608_PROV_PV_SIZE sizeof(IOTC_ECC608_PROV_DATA_PV_AZURE)

typedef struct DataHeader {
    uint16_t next : 9; // Offset of next header, up to 512 bytes (slot 8 is 416)
    uint16_t type : 7; // Type of current item, see provision_data.h
} DataHeader;

typedef union DataHeaderUnion {
    DataHeader header;
    uint8_t bytes[sizeof(DataHeader)];
} DataHeaderUnion;

// NOTE: Not indexed by type. Just an array one per type indexed by the order it is ecountered in the ECC608.
static char data_cache[IOTC_DATA_SLOT_SIZE];


static size_t ecchdr_get_data_size(DataHeaderUnion* h) {
    uint16_t this_header_offset = (char*) h - data_cache;
    return (h->header.next - this_header_offset - sizeof(DataHeaderUnion));
}

static DataHeaderUnion* ecchdr_next(DataHeaderUnion* h) {
    if (!h) {
        return (DataHeaderUnion*) data_cache;
    }
    return (DataHeaderUnion*) &data_cache[h->header.next];
}

static char* ecchdr_data_ptr(DataHeaderUnion* h) {
    return ((char*)h) + sizeof(DataHeaderUnion);
}

static DataHeaderUnion* append_iotconnect_blank_record(DataHeaderUnion* h, uint16_t type,  size_t data_size) {
    uint16_t this_header_offset = (char*) h - data_cache;
    h->header.next =  this_header_offset + sizeof(DataHeaderUnion) + (uint16_t) data_size;
    h->header.type = type;
    memset(ecchdr_data_ptr(h), 0, data_size);
    return ecchdr_next(h);
}

// DO NOT call this function if we already have any of these fields
static void append_iotconnect_blank_records(DataHeaderUnion* start) {
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_PLATFORM,  IOTC_ECC608_PROV_PV_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_CPID, IOTC_ECC608_PROV_CPID_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_DUID, IOTC_ECC608_PROV_DUID_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_ENV, IOTC_ECC608_PROV_ENV_SIZE);
    start->header.type = EMPTY;
    start->header.next = 0;
}

static ATCA_STATUS iotc_ecc608_get_string_value_internal(ecc_data_types data_type, char ** value) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    *value = NULL;
    switch (data_type) {
        case IOTC_ECC608_PROV_PLATFORM:
        case IOTC_ECC608_PROV_ENV:
        case IOTC_ECC608_PROV_CPID:
        case IOTC_ECC608_PROV_DUID:
            while(h->header.type != EMPTY) {
                if (h->header.type == data_type) {
                    *value = ecchdr_data_ptr(h);
                    return ATCA_SUCCESS;
                }
                h = ecchdr_next(h);
            }
            return ATCA_INVALID_ID;

        default:
            Log.error(F("IOTC_ECC608: Only IOTCONNECT data types are supported. Use copy_string_value instead."));
            return ATCA_BAD_PARAM;
    }
}

static ATCA_STATUS iotc_ecc608_set_string_value_internal(ecc_data_types data_type, const char * value) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    while(h->header.type != EMPTY) {
        if (h->header.type == data_type) {
            size_t size = ecchdr_get_data_size(h);
            switch (data_type) {
                case IOTC_ECC608_PROV_PLATFORM:
                case IOTC_ECC608_PROV_ENV:
                case IOTC_ECC608_PROV_CPID:
                case IOTC_ECC608_PROV_DUID:
                    // iotconnect data are null terminated strings
                    if ((strlen(value) + 1) > size) {
                        Log.error(F("IOTC_ECC608: String size is larger than reserved size!"));
                        return ATCA_INVALID_LENGTH;
                    }
                    strcpy(ecchdr_data_ptr(h), value);
                    break;
                default:
                    if ((strlen(value)) > size) {
                        Log.error(F("IOTC_ECC608: String size is larger than reserved size!"));
                        return ATCA_INVALID_LENGTH;
                    }
                    memcpy(ecchdr_data_ptr(h), value, size);
                    break;
            }
            return ATCA_SUCCESS;
        }
        h = ecchdr_next(h);
    }
    Log.error(F("IOTC_ECC608: Data type %d was not found in storage. Unable to write value.!"));
    return ATCA_INVALID_ID;
}

static ATCA_STATUS load_ecc608_cache(void) {
    ATCA_STATUS atca_status;
    size_t slot_size;
    // Determine slot size
    atca_status = atcab_get_zone_size(
        ATCA_ZONE_DATA,
        DATA_SLOT_NUM,
        &slot_size
    );
    if (ATCA_SUCCESS != atca_status)  {
        Log.errorf(F("IOTC_ECC608: Unable to read zone size: %d\n"), atca_status);
        return atca_status;
    }

    if (slot_size != IOTC_DATA_SLOT_SIZE) {
        Log.error(F("IOTC_ECC608: Unexpected data slot size received from ECC608!")); // Unlikely, so just a safeguard
    }

    atca_status = atcab_read_bytes_zone(
        ATCA_ZONE_DATA,
        DATA_SLOT_NUM,
        0,
        (uint8_t*) data_cache,
        slot_size
    );
    if (ATCA_SUCCESS != atca_status) {
        Log.error(F("IOTC_ECC608: Failed to read provisioning info!"));
        return atca_status;
    }

    bool has_iotconnect_data = false;
    int other_fields_count = 0;
    
    DataHeaderUnion* h = ecchdr_next(NULL);
    while(h->header.type != EMPTY) {
        switch(h->header.type) {
            case IOTC_ECC608_PROV_DUID: // fall through
            case IOTC_ECC608_PROV_CPID: // fall through
            case IOTC_ECC608_PROV_ENV:  // fall through
                other_fields_count++;   // fall through but increment count only for above
            case IOTC_ECC608_PROV_PLATFORM:
                Log.debugf("hdr %d Type: %d Size: %d Data: %s\n", (int)((char *) h - data_cache), h->header.type, (int) ecchdr_get_data_size(h), ecchdr_data_ptr(h));
                break;
            default:
                // don't garble with data
                Log.debugf("hdr %d Type: %d Size: %d\n", (int)((char *) h - data_cache), h->header.type, (int) ecchdr_get_data_size(h));            
        }
        // don't use switch for this case cause we want to break the while loop and not the switch
        if (h->header.type == IOTC_ECC608_PROV_PLATFORM) {
            if (has_iotconnect_data) {
                // Duplicater header?
                Log.errorf(F("IOTC_ECC608: Detected duplicate version header. Data corruption? Size: %d Value:%s\n"), (int) ecchdr_get_data_size(h), ecchdr_data_ptr(h));
                h->header.type = EMPTY;
                break;
            } else if (ecchdr_get_data_size(h) == IOTC_ECC608_PROV_PV_SIZE) {
                Log.debug("Size match");
                if (0 == strcmp(IOTC_ECC608_PROV_DATA_PV_1_0, ecchdr_data_ptr(h))) {
                    // convert the old value to the new version 2 scheme
                    // and assume Azure becasue the old version supported only azure
                    Log.info(F("IOTC_ECC608: Detected old version of ATECC608 data. Converted data to the new version."));
                    strcpy(ecchdr_data_ptr(h), IOTC_ECC608_PROV_DATA_PV_AZURE);
                    has_iotconnect_data = true;
                } else if (0 == strcmp(IOTC_ECC608_PROV_DATA_PV_AZURE, ecchdr_data_ptr(h))) {
                    Log.debug("Platform is Azure");
                    has_iotconnect_data = true;
                } else if (0 == strcmp(IOTC_ECC608_PROV_DATA_PV_AWS, ecchdr_data_ptr(h))) {
                    Log.debug("Platform is AWS");
                    has_iotconnect_data = true;
                } else {
                    Log.error(F("IOTC_ECC608: Unexpected iotconnect data version/platform value!"));
                }
            } else {
                Log.errorf(F("IOTC_ECC608: Expected data size %d but got %d!\n"), (int) IOTC_ECC608_PROV_PV_SIZE, (int) ecchdr_get_data_size(h));
            }
        }
        h = ecchdr_next(h);
        if ((char*)h > &data_cache[IOTC_DATA_SLOT_SIZE]) {
            Log.error(F("IOTC_ECC608: Could not find empty provisioning data header!")); // ran off past the end of data
            break;
        }
    }
    if (!has_iotconnect_data) {
        if (other_fields_count == 0) {
            append_iotconnect_blank_records(h);
        } else {
            Log.errorf(F("Expected no IoTConnect specific fields, but found %d. Unsupported version?\n"), other_fields_count);
            memset(data_cache, 0, sizeof(data_cache));
            h = ecchdr_next(NULL);
            append_iotconnect_blank_records(h);
        }        
        // make data sane:
        atca_status = iotc_ecc608_set_string_value_internal(IOTC_ECC608_PROV_PLATFORM, IOTC_ECC608_PROV_DATA_PV_AZURE);
    } else {
        if (other_fields_count != 3) {
            Log.errorf(F("Expected 4 IoTConnect specific fields, but found %d. Unsupported version? Resetting data\n"), other_fields_count + 1);
            memset(data_cache, 0, sizeof(data_cache));
            h = ecchdr_next(NULL);
            append_iotconnect_blank_records(h);
            atca_status = iotc_ecc608_set_string_value_internal(IOTC_ECC608_PROV_PLATFORM, IOTC_ECC608_PROV_DATA_PV_AZURE);
        }
        // reporpulate
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS iotc_ecc608_get_serial_as_string(char* serial_str) {
    ATCA_STATUS status;
    uint8_t serial_buffer[ATCA_SERIAL_NUM_SIZE];

    status = atcab_init(&cfg_atecc608b_i2c);
    if (status != ATCA_SUCCESS) {
        printf("atcab_init() failed: %02x\n", status);
        return status;
    }

    status = atcab_read_serial_number(serial_buffer);
    if (status != ATCA_SUCCESS) {
        printf("atcab_read_serial_number() failed: %02x\n", status);
        return status;
    }

    for (int i = 0; i < ATCA_SERIAL_NUM_SIZE; i++) {
        sprintf(&serial_str[i * 2], "%02X", serial_buffer[i]);
    }
    serial_str[ATCA_SERIAL_NUM_SIZE * 2] = 0;

    return ATCA_SUCCESS;
}

// for debugging purposes
void iotc_ecc608_dump_provision_data(void) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    while(h->header.type != EMPTY) {
        size_t size = ecchdr_get_data_size(h);
        char buffer[ECC608_MAX_DATA_ENTRY_SIZE];
        if (size > ECC608_MAX_DATA_ENTRY_SIZE) {
            Log.warn(F("IOTC_ECC608: WARNING: ECC608 provisioning entry larger than expected!"));
            buffer[0] = 0; // null terminate to empty
        } else {
            memcpy(buffer, ecchdr_data_ptr(h), size);
            buffer[size] = 0;
        }

        Log.infof(
            F("Type:%d, size:%u, next:%u %s\n"),
            h->header.type,
            ecchdr_get_data_size(h),
            h->header.next,
            buffer
        );
        h = ecchdr_next(h);
    }
}

ATCA_STATUS iotc_ecc608_init_provision(void) {
    ATCA_STATUS atca_status;

    // TODO: for some reason ECC608.begin() fails, which is better, sems to fail here
    atca_status = atcab_init(&cfg_atecc608b_i2c);
    if (atca_status != ATCA_SUCCESS) {
        Log.error(F("Failed to initialize ECC608!"));
        return atca_status;
    }
    atca_status = load_ecc608_cache();
    if (atca_status != ATCA_SUCCESS) {
        Log.error(F("failed to load ecc608 cache!"));
        return atca_status;
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS iotc_ecc608_get_string_value(ecc_data_types data_type, char ** value) {
    if (data_type == IOTC_ECC608_PROV_PLATFORM) {
        *value = NULL;
        Log.warn(F("IOTC_ECC608: Warning: User code should not be reading IOTC_ECC608_PROV_VER"));
    }
    return iotc_ecc608_get_string_value_internal(data_type, value);
}

ATCA_STATUS iotc_ecc608_get_platform(IotConnectConnectionType* type) {
    char * plaform_str;
    ATCA_STATUS status = iotc_ecc608_get_string_value_internal(IOTC_ECC608_PROV_PLATFORM, &plaform_str);
    if (ATCA_SUCCESS != status) {
        return status;
    }
    if (0 == strcmp(IOTC_ECC608_PROV_DATA_PV_AWS, plaform_str)) {
        *type = IOTC_CT_AWS;
    } else {
        *type = IOTC_CT_AZURE;
    }    
    return status;
}


// size is in/out: in=max / out=actual
ATCA_STATUS iotc_ecc608_copy_string_value(ecc_data_types data_type, char *buffer, size_t buffer_size) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    buffer[0] = 0;
    while(h->header.type != EMPTY) {
        if (h->header.type == data_type) {
            switch (data_type) {
                case IOTC_ECC608_PROV_PLATFORM: // iotc_ecc608_get_string_value will warn about this data_type
                case IOTC_ECC608_PROV_ENV:
                case IOTC_ECC608_PROV_CPID:
                case IOTC_ECC608_PROV_DUID:
                    ATCA_STATUS atca_status;
                    char* value;
                    atca_status = iotc_ecc608_get_string_value(data_type, &value);
                    if (ATCA_SUCCESS != atca_status) {
                        // called function will print the error
                        return atca_status;
                    }
                    if(buffer_size < (strlen(value) + 1)) {
                        return ATCA_INVALID_SIZE;
                    }
                    strcpy(buffer, value);
                    break;

                default:
                    if(buffer_size < (ecchdr_get_data_size(h) + 1)) {
                        Log.error(F("IOTC_ECC608: No room to copy the full value"));
                        return ATCA_INVALID_SIZE;
                    }
                    size_t data_size = ecchdr_get_data_size(h);
                    memcpy(buffer, ecchdr_data_ptr(h), data_size);
                    buffer[data_size] = 0;
                    break;
            } // end switch
            return ATCA_SUCCESS;
        }
        h = ecchdr_next(h);
    }
    return ATCA_INVALID_ID;
}

ATCA_STATUS iotc_ecc608_set_string_value(ecc_data_types data_type, const char * value) {
    if (data_type == IOTC_ECC608_PROV_PLATFORM) {
        Log.error(F("IOTC_ECC608: User code should not be writing IOTC_ECC608_PROV_PLATFORM directly"));
        return ATCA_BAD_PARAM;
    }
    return iotc_ecc608_set_string_value_internal(data_type, value);
}

ATCA_STATUS iotc_ecc608_set_platform(IotConnectConnectionType type) {
    if (type != IOTC_CT_AWS && type != IOTC_CT_AZURE) {
        Log.error(F("IOTC_ECC608: iotc_ecc608_set_platform() invalid argument"));
        return ATCA_BAD_PARAM;
    }
    const char* value = type == IOTC_CT_AWS ? IOTC_ECC608_PROV_DATA_PV_AWS : IOTC_ECC608_PROV_DATA_PV_AZURE;
    return iotc_ecc608_set_string_value_internal(IOTC_ECC608_PROV_PLATFORM, value);
}

ATCA_STATUS iotc_ecc608_write_all_data(void) {
    ATCA_STATUS atca_status;
    atca_status = atcab_write_bytes_zone(
        ATCA_ZONE_DATA,
        DATA_SLOT_NUM,
        0, // offset
        (uint8_t*) data_cache,
        IOTC_DATA_SLOT_SIZE // we already checked that the actual size matches
    );
    if (ATCA_SUCCESS != atca_status) {
        Log.errorf(F("Failed to write provisioning data! Error %d\n"), atca_status);
        return atca_status;
    }
    return ATCA_SUCCESS;
}

#if 0
// Keeping this function for reference only. It shows some usage examples.
void iotc_ecc608_unit_test (void){
    iotc_ecc608_init_provision();
    iotc_ecc608_dump_provision_data();
    #define BUFF_SIZE 130
    #define LONG_STRING "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
    char buffer[BUFF_SIZE];
    char* value;

    strcpy(buffer, LONG_STRING);
    if (iotc_ecc608_copy_string_value(AWS_THINGNAME, buffer, BUFF_SIZE)) { return; }
    Log.infof("AWS_THINGNAME: %s\r\n", buffer);

    Log.infof("Expected error: ");
    if (0 == iotc_ecc608_copy_string_value(AWS_THINGNAME, buffer, 5)) { return; }

    Log.infof("Expected warning: ");
    if (iotc_ecc608_copy_string_value(IOTC_ECC608_PROV_PLATFORM, buffer, BUFF_SIZE)) { return; }
    Log.infof("IOTC_ECC608_PROV_PLATFORM: %s\r\n", buffer);

    Log.infof("Expected warning: ");
    if (iotc_ecc608_get_string_value(IOTC_ECC608_PROV_PLATFORM, &value)) { return; }
    Log.infof("IOTC_ECC608_PROV_PLATFORM: %s\r\n", value);

    Log.infof("Expected error: ");
    if (0 == iotc_ecc608_set_string_value(IOTC_ECC608_PROV_PLATFORM, "v2.0")) { return; }

    Log.infof("Expected error: ");
    if (0 == iotc_ecc608_set_string_value(IOTC_ECC608_PROV_CPID, "12345678901234567890123456789012345678901234567890123456789012345678901234567890")) { return; }
    if (iotc_ecc608_set_string_value(IOTC_ECC608_PROV_CPID, "MY_CPID")) { return; }
    strcpy(buffer, LONG_STRING);
    if (iotc_ecc608_copy_string_value(IOTC_ECC608_PROV_CPID, buffer, BUFF_SIZE)) { return; }
    Log.infof("IOTC_ECC608_PROV_CPID: %s\r\n", buffer);
    if (iotc_ecc608_get_string_value(IOTC_ECC608_PROV_CPID, &value)) { return; }
    Log.infof("IOTC_ECC608_PROV_CPID: %s\r\n", value);

    Log.infof("Expected error: ");
    if (0 == iotc_ecc608_set_string_value(AZURE_ID_SCOPE, "dummy")) { return; }

    iotc_ecc608_dump_provision_data();

    //iotc_ecc608_write_all_data();
}
#endif