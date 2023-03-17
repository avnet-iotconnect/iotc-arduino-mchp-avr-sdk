#include <Arduino.h>
#include <atca_helpers.h>
#include <atcacert/atcacert_client.h>
#include <cryptoauthlib.h>
#include <log.h>
#include <ecc608.h>
#include <sequans_controller.h>

#include "cert_def_1_signer.h"
#include "cert_def_3_device.h"

#define AT_WRITE_CERTIFICATE "AT+SQNSNVW=\"certificate\",%u,%u"
#define AT_ERASE_CERTIFICATE "AT+SQNSNVW=\"certificate\",%u,0"

#define HTTP_CUSTOM_CA_SLOT   (15)
#define MQTT_CUSTOM_CA_SLOT   (16)

// NOTE the special modem-compatible format for certificates
#define CERT_GODADDY_ROOT_CA_G2 \
"\n-----BEGIN CERTIFICATE-----\n"\
"MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx"\
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT"\
"EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp"\
"ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz"\
"NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH"\
"EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE"\
"AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw"\
"DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD"\
"E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH"\
"/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy"\
"DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh"\
"GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR"\
"tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA"\
"AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE"\
"FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX"\
"WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu"\
"9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr"\
"gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo"\
"2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO"\
"LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI"\
"4uJEvlz36hz1"\
"\n-----END CERTIFICATE-----"

// NOTE the special modem-compatible format for certificates
#define CERT_BALTIMORE_ROOT_CA \
"\n-----BEGIN CERTIFICATE-----\n" \
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ" \
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD" \
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX" \
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y" \
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy" \
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr" \
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr" \
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK" \
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu" \
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy" \
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye" \
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1" \
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3" \
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92" \
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx" \
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0" \
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz" \
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS" \
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp" \
"\n-----END CERTIFICATE-----"

#define CUSTOM_CA_ROOT \
"\n-----BEGIN CERTIFICATE-----\n" \
"MIIDfTCCAmWgAwIBAgIJAKuM/BMFauavMA0GCSqGSIb3DQEBCwUAMFUxCzAJBgNV" \
"BAYTAlhYMRUwEwYDVQQHDAxEZWZhdWx0IENpdHkxHDAaBgNVBAoME0RlZmF1bHQg" \
"Q29tcGFueSBMdGQxETAPBgNVBAMMCEN1c3RvbUNBMB4XDTIzMDMwMTE2MDM1NVoX" \
"DTI0MDIyOTE2MDM1NVowVTELMAkGA1UEBhMCWFgxFTATBgNVBAcMDERlZmF1bHQg" \
"Q2l0eTEcMBoGA1UECgwTRGVmYXVsdCBDb21wYW55IEx0ZDERMA8GA1UEAwwIQ3Vz" \
"dG9tQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDbxzRUilhXY3vJ" \
"ciBUFMonkxtwH5fB7G35vzIN8Nt2xLn5/Y61lJuuCxN20XBAulUEkm5AIm4HN68G" \
"SvUX9GqqlGf1VpWTiWNEU7P19ScTB+kJdPWTPyI1+vzTFidmvW2E2vK7YxdnbFqO" \
"dVFXlb4LFPCeZjSAh5PF4G507ort3JWmBe9DdfWzkp+yOjjdLYUvB6o6To1Fb6SS" \
"X9s0gHj7+WCEThP9mFcT7K5/3bMAhuRunHY+xAVnAvTUdkFPqQCMdMNr6YsRhGjQ" \
"q12iZnWTu7S7OMjYZTZOryJkQ8Z3yHx4d+uueirZuk3sXyKJnihm3Gier6lSrz5Y" \
"kgygdtVHAgMBAAGjUDBOMB0GA1UdDgQWBBQOzmRi3/2pEArZEhMPDLf4GnVxLDAf" \
"BgNVHSMEGDAWgBQOzmRi3/2pEArZEhMPDLf4GnVxLDAMBgNVHRMEBTADAQH/MA0G" \
"CSqGSIb3DQEBCwUAA4IBAQAbgds99kJT96neV8rSoJ0+DQLiTA3ydzPd8hK7YTOw" \
"cYDmrE/d3MwrTtX3Cwe+tfZSHmBXNzrwoG/EWTA7IMSrq7UoYOhi0B3AodTruwUH" \
"idqVgsGKAtqlI4IDihTvAJWK4+UjkCDeLwyoH8dfHAZ6Kj4+yo/x5O+YipY4VqQM" \
"EkbgOdtB5s7e8UHQKUUd89CLIkbrpLzAsc05NMcmW2D5GQSZcLYG1z+wqNCEKNMK" \
"pSXdTBCQdffYlL6rD+J0+wzgP9XuGXlWPiQ5O4GvZ5oyBKs6MW0HBSJ+PpQo/PFg" \
"fck6aG/VEegYmcwgLHcnEjIeDwazGUYqhZr7R04vmP+u" \
"\n-----END CERTIFICATE-----"


static bool writeCaCertificate(const char* data, const uint8_t slot) {
    const size_t data_length = strlen(data);
    char command[48];
    char rbuff[4096];
    rbuff[0] = 0;

    SequansController.clearReceiveBuffer();

    sprintf(command, AT_ERASE_CERTIFICATE, slot);
    SequansController.writeCommand(command);

    sprintf(command, AT_WRITE_CERTIFICATE, slot, data_length);

    SequansController.writeBytes((uint8_t*)command, strlen(command), true);
    SequansController.waitForByte('>', 1000);
    SequansController.writeBytes((uint8_t*)data, data_length, true);

    ResponseResult res = SequansController.readResponse(rbuff, sizeof(rbuff));
    if (res != ResponseResult::OK) {
        Log.errorf("Write certificate error: %d. Response: \"%s\"\r\n", res, rbuff);
        return false;
    }

    return true;
}

static bool writCaCertificates() {
  SequansController.begin();

  if (!writeCaCertificate(CERT_GODADDY_ROOT_CA_G2, HTTP_CUSTOM_CA_SLOT)) {
    Log.error("Unable to store the HTTP CA certificate!");
    return false;
  }
  Log.info("HTTPS CA certificate updated successfuly.");
  if (!writeCaCertificate(CUSTOM_CA_ROOT, MQTT_CUSTOM_CA_SLOT)) {
    Log.error("Unable to store the MQTT CA certificate!");
    return false;
  }
  Log.info("MQTT CA certificate updated successfuly.");
  return true;
}

static bool printConfigValues() {
    uint8_t status;
    char duid[25]; // 20 bytes from thing name + "avr-"
    char thingName[128] = {0};
    uint8_t thingNameLen = sizeof(thingName);

    // Get the thingname
    status = ECC608.getThingName((uint8_t *)thingName, &thingNameLen);
    if (status != ECC608.ERR_OK) {
        Log.error("Could not retrieve thing name from the ECC");
        return false;
    }
    if (strlen(thingName) < 20) {
        Log.error("The thing name does not seem to be correct!");
        return false;

    }
    strcpy(duid, "avr-");
    strcat(duid, (char*)&thingName[strlen(thingName)-20]);
    
    Log.rawf("Device ID is %s. Raw value %s)\r\n", duid, thingName);

    return true;
}

static void printCertificate(uint8_t* certificate, uint16_t size) {
    char buffer[1024];
    size_t buffer_size = sizeof(buffer);
    ATCA_STATUS result =
        atcab_base64encode(certificate, size, buffer, &buffer_size);

    if (result != ATCA_SUCCESS) {
        Log.errorf("Failed to encode into base64: %x\r\n", result);
        return;
    }

    buffer[buffer_size] = 0;
    Log.rawf(
        "-----BEGIN CERTIFICATE-----\r\n%s\r\n-----END CERTIFICATE-----\r\n",
        buffer);
}


void setup() {
    Log.begin(115200);

    int status;

    if (!writCaCertificates()) {
      return;
    }

    static ATCAIfaceCfg cfg_atecc608b_i2c = {ATCA_I2C_IFACE,
                                             ATECC608B,
                                             {
                                                 0x58,  // 7 bit address of ECC
                                                 2,     // Bus number
                                                 100000 // Baud rate
                                             },
                                             1560,
                                             20,
                                             NULL};

    if (ATCA_SUCCESS != (status = atcab_init(&cfg_atecc608b_i2c))) {
        Log.errorf("Failed to init: %d", status);
        return;
    } else {
        Log.info("Initialized ECC608");
    }

    if (!printConfigValues()) {
      return;
    }

    // Retrieve public root key
    uint8_t public_key[ATCA_PUB_KEY_SIZE];
    if (ATCA_SUCCESS != (status = atcab_get_pubkey(0, public_key))) {
        Log.errorf("Failed to get public key: %x", status);
        return;
    }

    uint8_t buffer[g_cert_def_1_signer.cert_template_size + 4];
    size_t size = sizeof(buffer);

    // Retrive device certificate
    if (ATCA_SUCCESS != (status = atcacert_read_cert(&g_cert_def_3_device,
                                                     public_key,
                                                     buffer,
                                                     &size))) {
        Log.errorf("Failed to read device certificate: %d", status);
        return;
    } else {
        Log.info("Device certificate:");
        printCertificate(buffer, size);
    }

    size = sizeof(buffer);
    // Retrive sign certificate
    if (ATCA_SUCCESS != (status = atcacert_read_cert(&g_cert_def_1_signer,
                                                     public_key,
                                                     buffer,
                                                     &size))) {
        Log.errorf("Failed to read signing certificate: %d", status);
        return;
    } else {
        Log.info("Signing Certificate:");
        printCertificate(buffer, size);
    }
}

void loop() {}
