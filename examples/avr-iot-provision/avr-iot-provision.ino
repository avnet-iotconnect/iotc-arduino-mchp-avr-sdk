#include <Arduino.h>
//#include <atcacert/atcacert_client.h>
//#include <cryptoauthlib.h>
#include <log.h>
#include <ecc608.h>
#include <sequans_controller.h>

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

static bool writeServerCaCertificate(const char* data, const uint8_t slot) {
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

static bool writeCiphersuiteConfig() {
  // This section definiton matches the list in provision.ino sample provided by the AVR-IoT-Cellular Library
  // Ultimately, the command should be AT+SQNSPCFG=1,2,"0xC027",1,16,18,0,"","",1 when using TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
  //const char* AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC = "AT+SQNSPCFG=1,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\",1";
  const char* AT_MQTT_SECURITY_PROFILE_WITH_CERTIFICATES_ECC = "AT+SQNSPCFG=1,%u,\"%s\",%u,%u,%u,%u,\"%s\",\"%s\",1";
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
    MQTT_PUBLIC_KEY_SLOT,+
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
    Log.error("writeCiphersuiteConfig: Unable to communcate withth the modem!");
    return false;
  }
  Log.info("Ciphersuites config written successfully.");    
  return true;
}

static bool writeHttpsScurityProfile() {
  const char* const AT_HTTPS_SECURITY_PROFILE = "AT+SQNSPCFG=3,%u,\"\",%u,%u";
  const unsigned int ca_index = HTTP_CUSTOM_CA_SLOT;
  const unsigned int TLS_v1_2 = 2;

  char command[strlen(AT_HTTPS_SECURITY_PROFILE) + 64] = "";

  sprintf(command, AT_HTTPS_SECURITY_PROFILE, TLS_v1_2, 1, ca_index);

  SequansController.writeBytes((uint8_t*)command, strlen(command), true);

  // Wait for URC confirming the security profile
  if (!SequansController.waitForURC("SQNSPCFG", NULL, 0, 4000)) {
      Log.error("Error whilst doing the provisioning");
      return false;
  }

  SequansController.clearReceiveBuffer();
  return true;
}

static bool writCaCertificates() {
  SequansController.begin();

  if (!writeServerCaCertificate(CERT_GODADDY_ROOT_CA_G2, HTTP_CUSTOM_CA_SLOT)) {
    Log.error("Unable to store the HTTP CA certificate!");
    return false;
  }
  Log.info("HTTPS CA certificate updated successfuly.");
  if (!writeServerCaCertificate(CERT_BALTIMORE_ROOT_CA, MQTT_CUSTOM_CA_SLOT)) {
    Log.error("Unable to store the MQTT CA certificate!");
    return false;
  }
  Log.info("MQTT CA certificate updated successfuly.");
  return true;
}

static bool printConfigValues() {
  ATCA_STATUS status;
  char duid[84]; // 20 bytes from thing name + "avr-"
  uint8_t thingName[80];
  size_t thingNameLen = sizeof(thingName);

  // Get the thingname
  status = ECC608.readProvisionItem(AWS_THINGNAME, (uint8_t *)thingName, &thingNameLen);
  if (status != ATCACERT_E_SUCCESS) {
    Log.error("Could not retrieve thing name from the ECC");
    return false;
  }
  if (thingNameLen < 20) {
    Log.warn("The thing name exceeds 20 characters");
    return true;
  }

  thingName[thingNameLen] = 0; // make sure to null-terminate the thing name from ECC608
  strcpy(duid, "avr-");

  // need to truncate the thing name length due to IoTConnect UI restricition.
  // Assuming that bytes to the right will give us more randomization.
  strcat(duid, (char*)&thingName[thingNameLen-20]);
  
  Log.rawf("Device ID is %s.\r\n", duid, thingName);

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
      buffer
  );
}
bool loadAndPrintCertificates() {
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
    Log.errorf("ERROR: Fevice certificate is %lu bytes in size, but the buffer is only  %lu bytes.\r\n",
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

  printCertificate(certificate_buffer, device_certificate_size);
  return true;
}


void setup() {
  Log.begin(115200);

  int status;

  if (!writCaCertificates()) {
    return;
  }

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

  if (ATCA_SUCCESS != (status = atcab_init(&cfg_atecc608b_i2c))) {
    Log.errorf("Failed to init ECC608: %d\r\n", status);
      return;
  } else {
    Log.info("Initialized ECC608");
  }

  // In case of error, error will be printed by the called function 
  // in all these cases below
  if(!writeCiphersuiteConfig()) {
    return;
  }
  if (!printConfigValues()) {
    return;
  }
  if (!loadAndPrintCertificates()) {
    return;
  }
  if (!writeHttpsScurityProfile()) {
    return;
  }

  Log.info("========= Provisioning Complete =========");
}

void loop() {
  delay(10000);
}
