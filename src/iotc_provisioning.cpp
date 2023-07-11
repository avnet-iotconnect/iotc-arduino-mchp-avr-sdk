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
// Created by Nik Markovic <nikola.markovic@avnet.com> on 5/22/23.
//

#include <Arduino.h>
#include <log.h>
#include "ecc608.h"
#include "cryptoauthlib/app/tng/tng_atcacert_client.h"
#include "sequans_controller.h"
#include "iotc_ecc608.h"

#define AT_WRITE_CERTIFICATE "AT+SQNSNVW=\"certificate\",%u,%u"
#define AT_ERASE_CERTIFICATE "AT+SQNSNVW=\"certificate\",%u,0"

#define HTTP_CUSTOM_CA_SLOT   (15)
#define MQTT_CUSTOM_CA_SLOT   (16)
#define MQTT_PUBLIC_KEY_SLOT  (0)
#define MQTT_PRIVATE_KEY_SLOT (0)

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

static bool write_ca_server_certificate(const char* data, const uint8_t slot) {
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

static bool write_ciphersuite_config(void) {
  // This section definiton matches the list in provision.ino sample provided by the AVR-IoT-Cellular Library
  // Ultimately, the command should be AT+SQNSPCFG=1,2,"0xC027",1,16,18,0,"","",1 when using TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
  //const char* AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC = "AT+SQNSPCFG=1,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\",1";
  const char* AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC = "AT+SQNSPCFG=2,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\",1";
  const char* CIPHER49 = "0xC027"; // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
  const unsigned int TLS_v1_2 = 2;
  const char* psk = "";
  const char* psk_identity = "";
  const unsigned int ca_index = MQTT_CUSTOM_CA_SLOT;

  // Roughly overestimating the string size... Because of a few %u's and empty %s's
  // with CIPHER49 being 6 characters and extra null, it should fit
  const size_t command_size = strlen(AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC);
  char command[command_size] = "";

  snprintf(command,
    command_size,
    AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC,
    TLS_v1_2,
    CIPHER49,
    1,
    ca_index,
    MQTT_PUBLIC_KEY_SLOT,
    MQTT_PRIVATE_KEY_SLOT,
    psk,
    psk_identity
  );
  // Just in case someone made a mistake on size. Let's not walk over unowned memory.
  command[command_size] = '\0';
  SequansController.writeBytes((uint8_t*)command,
    strlen(command),
    true
  );

  // Wait for URC confirming the security profile
  if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
    Log.error(F("writeCiphersuiteConfig: Unable to communicate with the modem!"));
    return false;
  }
  Log.info(F("Ciphersuites config written successfully."));
  return true;
}

static bool write_http_security_profile(void) {
  const char* const AT_HTTPS_SECURITY_PROFILE = "AT+SQNSPCFG=3,%u,\"\",%u,%u";
  const unsigned int ca_index = HTTP_CUSTOM_CA_SLOT;
  const unsigned int TLS_v1_2 = 2;

  char command[strlen(AT_HTTPS_SECURITY_PROFILE) + 64] = "";

  sprintf(command, AT_HTTPS_SECURITY_PROFILE, TLS_v1_2, 1, ca_index);

  SequansController.writeBytes((uint8_t*)command, strlen(command), true);

  // Wait for URC confirming the security profile
  if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
      Log.error(F("Error whilst doing the provisioning"));
      return false;
  }

  SequansController.clearReceiveBuffer();
  return true;
}

static bool write_ca_server_certificates(void) {
  SequansController.begin();

  if (!write_ca_server_certificate(CERT_GODADDY_ROOT_CA_G2, HTTP_CUSTOM_CA_SLOT)) {
    Log.error(F("Unable to store the HTTP CA certificate!"));
    return false;
  }
  Log.info(F("HTTPS CA certificate updated successfuly."));
  if (!write_ca_server_certificate(CERT_BALTIMORE_ROOT_CA, MQTT_CUSTOM_CA_SLOT)) {
    Log.error(F("Unable to store the MQTT CA certificate!"));
    return false;
  }
  Log.info(F("MQTT CA certificate updated successfuly."));
  return true;
}

static void print_certificate(uint8_t* certificate, uint16_t size) {
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
      buffer
  );
}

bool print_device_certificate() {
  uint8_t certificate_buffer[1024];
  size_t device_certificate_size_max = 0;
  size_t device_certificate_size = 0;
  int atca_cert_status;
  const size_t certificate_buffer_size = sizeof(certificate_buffer);

  atca_cert_status = ECC608.getDeviceCertificateSize(
    &device_certificate_size_max
  );
  if (atca_cert_status != ATCACERT_E_SUCCESS) {
    Log.errorf("Failed to get device certificate's max size, status code: "
      "0x%x\r\n",
      atca_cert_status
      );
    return false;
  }
  if (device_certificate_size_max > certificate_buffer_size) {
    Log.errorf("ERROR: Device certificate is %lu bytes in size, but the buffer is only  %lu bytes.\r\n",
      device_certificate_size_max,
      certificate_buffer_size
    );
    return false;
  }
  device_certificate_size = certificate_buffer_size;
  atca_cert_status = ECC608.getDeviceCertificate(
      certificate_buffer,
      &device_certificate_size
  );
  if (atca_cert_status != ATCACERT_E_SUCCESS) {
      Log.errorf("Failed to get device certificate, status code: "
                  "0x%X\r\n",
                  atca_cert_status);
      return false;
  }
  print_certificate(certificate_buffer, device_certificate_size);
  return true;
}


void iotc_prov_init(void) {
    ECC608.begin();
    SequansController.begin();
}

bool iotc_prov_setup_tls_and_server_certs(void) {
    return write_ciphersuite_config()
        && write_http_security_profile()
        && write_ca_server_certificates()
    ;
}

void iotc_prov_print_device_certificate(void) {
    print_device_certificate();
}
