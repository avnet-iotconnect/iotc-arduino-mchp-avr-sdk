//
// Copyright: Avnet, Microchip Technology Inc 2023
// Created by Nik Markovic <nikola.markovic@avnet.com> on 5/16/21.
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

#define IOTC_ECC608_PROV_DATA_VERSION "v1.0" // Current IOTC_ECC608_PROV_VER. This is internal. User should not use this value.
#define IOTC_ECC608_PROV_DUID_SIZE 66
#define IOTC_ECC608_PROV_CPID_SIZE 66
#define IOTC_ECC608_PROV_ENV_SIZE 20
#define IOTC_ECC608_PROV_VER_SIZE (sizeof(IOTC_ECC608_PROV_DATA_VERSION))

struct DataHeader {
    uint16_t next : 9; // Offset of next header, up to 512 bytes (slot 8 is 416)
    uint16_t type : 7; // Type of current item, see provision_data.h
};

typedef union DataHeaderUnion {
    struct DataHeader header;
    uint8_t bytes[sizeof(struct DataHeader)];
} DataHeaderUnion;

// NOTE: Not indexed by type. Just an array one per type indexed by the order it is ecountered in the ECC608.
static char data_cache[IOTC_DATA_SLOT_SIZE];



static ATCA_STATUS init_ecc608(void) {
    ATCA_STATUS atca_status;
    if (ATCA_SUCCESS != (atca_status = atcab_init(&cfg_atecc608b_i2c))) {
        Log.errorf("Failed to init ECC608: %d\r\n", atca_status);
    } else {
        Log.info("Initialized ECC608");
    }
    return atca_status;
}

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
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_VER,  IOTC_ECC608_PROV_VER_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_CPID, IOTC_ECC608_PROV_CPID_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_DUID, IOTC_ECC608_PROV_DUID_SIZE);
    start = append_iotconnect_blank_record(start, IOTC_ECC608_PROV_ENV,  IOTC_ECC608_PROV_ENV_SIZE);
    start->header.type = EMPTY;
    start->header.next = 0;
}

static ATCA_STATUS iotc_ecc608_get_string_value_internal(ecc_data_types data_type, char ** value) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    *value = NULL;
    switch (data_type) {
        case IOTC_ECC608_PROV_VER:
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
            Log.error("IOTC_ECC608: Only IOTCONNECT data types are supported. Use copy_string_value instead.");
            return ATCA_BAD_PARAM;
    }
}

static ATCA_STATUS iotc_ecc608_set_string_value_internal(ecc_data_types data_type, const char * value) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    while(h->header.type != EMPTY) {
        if (h->header.type == data_type) {
            size_t size = ecchdr_get_data_size(h);
            switch (data_type) {
                case IOTC_ECC608_PROV_VER:
                case IOTC_ECC608_PROV_ENV:
                case IOTC_ECC608_PROV_CPID:
                case IOTC_ECC608_PROV_DUID:
                    // iotconnect data re null terminated strings
                    if ((strlen(value) + 1) > size) {
                        Log.error("IOTC_ECC608: String size is larger than reserved size!");
                        return ATCA_INVALID_LENGTH;
                    }
                    strcpy(ecchdr_data_ptr(h), value);
                    break;
                default:
                    if ((strlen(value)) > size) {
                        Log.error("IOTC_ECC608: String size is larger than reserved size!");
                        return ATCA_INVALID_LENGTH;
                    }
                    memcpy(ecchdr_data_ptr(h), value, size);
                    break;
            }
            return ATCA_SUCCESS;
        }
        h = ecchdr_next(h);
    }
    Log.error("IOTC_ECC608: Data type %d was not found in storage. Unable to write value.!");
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
        Log.errorf("IOTC_ECC608: Unable to read zone size: %d\r\n", atca_status);
        return atca_status;
    }

    if (slot_size != IOTC_DATA_SLOT_SIZE) {
        Log.error("IOTC_ECC608: Unexpected data slot size received from ECC608!"); // Unlikely, so just a safeguard
    }

    atca_status = atcab_read_bytes_zone(
        ATCA_ZONE_DATA,
        DATA_SLOT_NUM,
        0,
        (uint8_t*) data_cache,
        slot_size
    );
    if (ATCA_SUCCESS != atca_status) {
        Log.error("IOTC_ECC608: Failed to read provisioning info!");
        return atca_status;
    }

    bool has_iotconnect_data = false;
    DataHeaderUnion* h = ecchdr_next(NULL);
    while(h->header.type != EMPTY) {
        if (h->header.type == IOTC_ECC608_PROV_VER) {
            if (ecchdr_get_data_size(h) != sizeof(IOTC_ECC608_PROV_DATA_VERSION)
                || 0 != strcmp(IOTC_ECC608_PROV_DATA_VERSION, ecchdr_data_ptr(h))
            ) {
                Log.error("IOTC_ECC608: Unexpected iotconnect data version!");
            }
            has_iotconnect_data = true; // all or nothing
        }
        h = ecchdr_next(h);
        if ((char*)h > &data_cache[IOTC_DATA_SLOT_SIZE]) {
            Log.error("IOTC_ECC608: Could not find empty provisioning data header!"); // ran off past the end of data
        }
    }
    if (!has_iotconnect_data) {
        append_iotconnect_blank_records(h);
        atca_status = iotc_ecc608_set_string_value_internal(IOTC_ECC608_PROV_VER, IOTC_ECC608_PROV_DATA_VERSION);

    }
    return ATCA_INVALID_ID;
}

// for debugging purposes
void iotc_ecc608_dump_provision_data(void) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    while(h->header.type != EMPTY) {
        size_t size = ecchdr_get_data_size(h);
        char buffer[ECC608_MAX_DATA_ENTRY_SIZE];
        if (size > ECC608_MAX_DATA_ENTRY_SIZE) {
            Log.warn("IOTC_ECC608: WARNING: ECC608 provisioning entry larger than expected!");
            buffer[0] = 0; // null terminate to empty
        } else {
            memcpy(buffer, ecchdr_data_ptr(h), size);
            buffer[size] = 0;
        }

        Log.infof(
            "Type:%d, size:%u, next:%u %s\r\n",
            h->header.type,
            ecchdr_get_data_size(h),
            h->header.next,
            buffer
        );
        h = ecchdr_next(h);
    }
}

ATCA_STATUS iotc_ecc608_init_provision(void) {
    ATCA_STATUS atca_status = init_ecc608();
    if (atca_status != ATCA_SUCCESS) {
        return atca_status;
    }
    atca_status = load_ecc608_cache();
    if (atca_status != ATCA_SUCCESS) {
        return atca_status;
    }
    return ATCA_SUCCESS;
}

ATCA_STATUS iotc_ecc608_get_string_value(ecc_data_types data_type, char ** value) {
    if (data_type == IOTC_ECC608_PROV_VER) {
        *value = NULL;
        Log.warn("IOTC_ECC608: Warning: User code should not be reading IOTC_ECC608_PROV_VER");
    }
    return iotc_ecc608_get_string_value_internal(data_type, value);
}

// size is in/out: in=max / out=actual
ATCA_STATUS iotc_ecc608_copy_string_value(ecc_data_types data_type, char *buffer, size_t buffer_size) {
    DataHeaderUnion* h = (DataHeaderUnion *) data_cache;
    buffer[0] = 0;
    while(h->header.type != EMPTY) {
        if (h->header.type == data_type) {
            switch (data_type) {
                case IOTC_ECC608_PROV_VER: // iotc_ecc608_get_string_value will warn about this data_type
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
                        Log.error("IOTC_ECC608: No room to copy the full value");
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
    if (data_type == IOTC_ECC608_PROV_VER) {
        Log.error("IOTC_ECC608: Warning: User code should not be writing IOTC_ECC608_PROV_VER");
        return ATCA_BAD_PARAM;
    }
    return iotc_ecc608_set_string_value_internal(data_type, value);
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
        Log.errorf("Failed to write provisioning data! Error %d\r\n", atca_status);
        return atca_status;
    }
    return ATCA_SUCCESS;
}

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
    if (iotc_ecc608_copy_string_value(IOTC_ECC608_PROV_VER, buffer, BUFF_SIZE)) { return; }
    Log.infof("IOTC_ECC608_PROV_VER: %s\r\n", buffer);

    Log.infof("Expected warning: ");
    if (iotc_ecc608_get_string_value(IOTC_ECC608_PROV_VER, &value)) { return; }
    Log.infof("IOTC_ECC608_PROV_VER: %s\r\n", value);

    Log.infof("Expected error: ");
    if (0 == iotc_ecc608_set_string_value(IOTC_ECC608_PROV_VER, "v2.0")) { return; }

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