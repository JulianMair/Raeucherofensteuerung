/*
 * Tests der Prüfung von SSID und WLAN-Passwort aus firmware/net
 * (NET-01, NET-04, E-041).
 */
#include "unity.h"

#include "wifi_credentials.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_valid_credentials_are_accepted(void)
{
    TEST_ASSERT_EQUAL(NET_WIFI_OK, net_wifi_check("Raeucherofen", "geheim123"));
    TEST_ASSERT_EQUAL(NET_WIFI_OK, net_wifi_check("R", "12345678"));
}

/* NET-04: Mit dem Platzhalter aus dem Repository startet kein Access Point */
static void test_placeholder_password_is_rejected(void)
{
    TEST_ASSERT_EQUAL(NET_WIFI_PASSWORD_IS_PLACEHOLDER, net_wifi_check("Raeucherofen", NET_WIFI_PASSWORD_PLACEHOLDER));
}

/* WPA2 verlangt 8 bis 63 Zeichen */
static void test_password_length_is_checked(void)
{
    char password[NET_WIFI_PASSWORD_MAX + 2];
    memset(password, 'x', sizeof(password));

    password[NET_WIFI_PASSWORD_MIN - 1] = '\0';
    TEST_ASSERT_EQUAL(NET_WIFI_PASSWORD_LENGTH, net_wifi_check("R", password));
    TEST_ASSERT_EQUAL(NET_WIFI_PASSWORD_LENGTH, net_wifi_check("R", ""));

    memset(password, 'x', sizeof(password));
    password[NET_WIFI_PASSWORD_MAX] = '\0';
    TEST_ASSERT_EQUAL(NET_WIFI_OK, net_wifi_check("R", password));

    memset(password, 'x', sizeof(password));
    password[NET_WIFI_PASSWORD_MAX + 1] = '\0';
    TEST_ASSERT_EQUAL(NET_WIFI_PASSWORD_LENGTH, net_wifi_check("R", password));
}

/* Eine SSID hat 1 bis 32 Byte */
static void test_ssid_length_is_checked(void)
{
    char ssid[NET_WIFI_SSID_MAX + 2];
    memset(ssid, 's', sizeof(ssid));

    TEST_ASSERT_EQUAL(NET_WIFI_SSID_INVALID, net_wifi_check("", "geheim123"));

    ssid[NET_WIFI_SSID_MAX] = '\0';
    TEST_ASSERT_EQUAL(NET_WIFI_OK, net_wifi_check(ssid, "geheim123"));

    memset(ssid, 's', sizeof(ssid));
    ssid[NET_WIFI_SSID_MAX + 1] = '\0';
    TEST_ASSERT_EQUAL(NET_WIFI_SSID_INVALID, net_wifi_check(ssid, "geheim123"));
}

static void test_every_result_has_a_message(void)
{
    TEST_ASSERT_NOT_EQUAL(0, strlen(net_wifi_check_message(NET_WIFI_OK)));
    TEST_ASSERT_NOT_EQUAL(0, strlen(net_wifi_check_message(NET_WIFI_SSID_INVALID)));
    TEST_ASSERT_NOT_EQUAL(0, strlen(net_wifi_check_message(NET_WIFI_PASSWORD_IS_PLACEHOLDER)));
    TEST_ASSERT_NOT_EQUAL(0, strlen(net_wifi_check_message(NET_WIFI_PASSWORD_LENGTH)));
    TEST_ASSERT_NOT_EQUAL(0, strlen(net_wifi_check_message((net_wifi_check_t)99)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_credentials_are_accepted);
    RUN_TEST(test_placeholder_password_is_rejected);
    RUN_TEST(test_password_length_is_checked);
    RUN_TEST(test_ssid_length_is_checked);
    RUN_TEST(test_every_result_has_a_message);
    return UNITY_END();
}
