#pragma once

// ─────────────────────────────────────────────────────────────
// SECRETS TEMPLATE
// 1. Copy this file and rename the copy to secrets.h
// 2. Fill in your actual values in secrets.h
// 3. Never commit secrets.h — it is gitignored
// ─────────────────────────────────────────────────────────────

#define WIFI_SSID        "your_wifi_ssid"
#define WIFI_PASSWORD    "your_wifi_password"
#define AWS_IOT_ENDPOINT "xxxxxxxxxxxx-ats.iot.region.amazonaws.com"

static const char AWS_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
< paste contents of AmazonRootCA1.pem here >
-----END CERTIFICATE-----
)EOF";

static const char DEVICE_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
< paste contents of xxx-certificate.pem.crt here >
-----END CERTIFICATE-----
)EOF";

static const char DEVICE_KEY[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
< paste contents of xxx-private.pem.key here >
-----END RSA PRIVATE KEY-----
)EOF";