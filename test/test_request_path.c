/*
 * Tests der Pfadzerlegung aus firmware/net (API-02, NET-02, NET-03, E-039).
 * Reines C, deshalb auf dem Host prüfbar, obwohl es zum Webserver gehört.
 */
#include "unity.h"

#include "request_path.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static char id_text[ROF_PROTO_SENSOR_ID_LEN + 1];

/* ---- Sensorpfade --------------------------------------------------------- */

static void test_role_and_offset_paths_are_recognised(void)
{
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_ROLE, net_parse_sensor_path("/api/sensors/28AA000000000001/role", id_text));
    TEST_ASSERT_EQUAL_STRING("28AA000000000001", id_text);
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_OFFSET, net_parse_sensor_path("/api/sensors/28BB000000000002/offset", id_text));
    TEST_ASSERT_EQUAL_STRING("28BB000000000002", id_text);
}

/* Der Abfrageteil gehört nicht zum Pfad */
static void test_query_is_ignored(void)
{
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_ROLE, net_parse_sensor_path("/api/sensors/28AA000000000001/role?x=1", id_text));
    TEST_ASSERT_EQUAL_STRING("28AA000000000001", id_text);
}

/* Die ID prüft proto (bad_sensor_id). Hat sie nicht 16 Zeichen, bleibt sie
 * leer, damit nichts abgeschnitten wird und dadurch gültig aussieht. */
static void test_id_of_wrong_length_is_passed_on_empty(void)
{
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_ROLE, net_parse_sensor_path("/api/sensors/28AA0000000000011/role", id_text));
    TEST_ASSERT_EQUAL_STRING("", id_text);
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_ROLE, net_parse_sensor_path("/api/sensors/28AA/role", id_text));
    TEST_ASSERT_EQUAL_STRING("", id_text);
}

/* Kleinbuchstaben gibt die Zerlegung weiter, ablehnen muss sie proto */
static void test_id_content_is_not_checked_here(void)
{
    TEST_ASSERT_EQUAL(NET_SENSOR_PATH_OFFSET, net_parse_sensor_path("/api/sensors/28aa000000000001/offset", id_text));
    TEST_ASSERT_EQUAL_STRING("28aa000000000001", id_text);
}

static void test_other_paths_do_not_match(void)
{
    const char *paths[] = {
        "/api/sensors/28AA000000000001",       "/api/sensors/28AA000000000001/",
        "/api/sensors/28AA000000000001/name",  "/api/sensors//role",
        "/api/sensors/28AA000000000001/role/x", "/api/sensor/28AA000000000001/role",
        "/api/sensors/28AA000000000001/roles", "/api/state",
        "",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        TEST_ASSERT_EQUAL_MESSAGE(NET_SENSOR_PATH_NO_MATCH, net_parse_sensor_path(paths[i], id_text), paths[i]);
    }
}

/* ---- API oder Datei ------------------------------------------------------ */

static void test_api_paths_are_recognised(void)
{
    TEST_ASSERT_TRUE(net_is_api_path("/api/state"));
    TEST_ASSERT_TRUE(net_is_api_path("/api/unknown"));
    TEST_ASSERT_TRUE(net_is_api_path("/api"));
    TEST_ASSERT_TRUE(net_is_api_path("/api?x"));
    TEST_ASSERT_FALSE(net_is_api_path("/apix"));
    TEST_ASSERT_FALSE(net_is_api_path("/"));
    TEST_ASSERT_FALSE(net_is_api_path("/generate_204"));
}

/* ---- Statische Dateien (NET-03) ------------------------------------------ */

static net_static_file_t file;

static void test_root_is_index_page(void)
{
    TEST_ASSERT_TRUE(net_static_file_for("/", &file));
    TEST_ASSERT_EQUAL_STRING("/www/index.html.gz", file.path);
    TEST_ASSERT_EQUAL_STRING("text/html; charset=utf-8", file.content_type);
}

static void test_files_map_to_gzip_files(void)
{
    TEST_ASSERT_TRUE(net_static_file_for("/assets/app-1a2b.js?v=3", &file));
    TEST_ASSERT_EQUAL_STRING("/www/assets/app-1a2b.js.gz", file.path);
    TEST_ASSERT_EQUAL_STRING("application/javascript", file.content_type);

    TEST_ASSERT_TRUE(net_static_file_for("/sub/", &file));
    TEST_ASSERT_EQUAL_STRING("/www/sub/index.html.gz", file.path);
}

static void test_content_types(void)
{
    TEST_ASSERT_EQUAL_STRING("text/css", net_content_type("/www/a.css.gz"));
    TEST_ASSERT_EQUAL_STRING("image/svg+xml", net_content_type("/www/a.svg.gz"));
    TEST_ASSERT_EQUAL_STRING("application/json", net_content_type("/www/a.json.gz"));
    TEST_ASSERT_EQUAL_STRING("image/x-icon", net_content_type("/www/favicon.ico.gz"));
    TEST_ASSERT_EQUAL_STRING("image/png", net_content_type("/www/a.png.gz"));
    TEST_ASSERT_EQUAL_STRING("font/woff2", net_content_type("/www/a.woff2.gz"));
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", net_content_type("/www/a.bin.gz"));
    TEST_ASSERT_EQUAL_STRING("application/octet-stream", net_content_type("/www/noext.gz"));
}

/* Kein Weg aus /www heraus und keine seltsamen Zeichen */
static void test_unsafe_paths_are_rejected(void)
{
    const char *paths[] = {
        "/../nvs",     "/a/../../x", "/a/..",        "//index.html", "/a//b.js",
        "/a\\b.js",    "/a b.js",    "/a%2e%2e/b",   "/ä.html",      "",
        "index.html",  "/a\x01.js",
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        TEST_ASSERT_FALSE_MESSAGE(net_static_file_for(paths[i], &file), paths[i]);
    }
}

static void test_too_long_path_is_rejected(void)
{
    char uri[NET_URI_PATH_MAX + 2];
    memset(uri, 'a', sizeof(uri) - 1);
    uri[0] = '/';
    uri[sizeof(uri) - 1] = '\0';
    TEST_ASSERT_FALSE(net_static_file_for(uri, &file));

    uri[NET_URI_PATH_MAX] = '\0'; /* genau so lang wie erlaubt */
    TEST_ASSERT_TRUE(net_static_file_for(uri, &file));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_role_and_offset_paths_are_recognised);
    RUN_TEST(test_query_is_ignored);
    RUN_TEST(test_id_of_wrong_length_is_passed_on_empty);
    RUN_TEST(test_id_content_is_not_checked_here);
    RUN_TEST(test_other_paths_do_not_match);
    RUN_TEST(test_api_paths_are_recognised);
    RUN_TEST(test_root_is_index_page);
    RUN_TEST(test_files_map_to_gzip_files);
    RUN_TEST(test_content_types);
    RUN_TEST(test_unsafe_paths_are_rejected);
    RUN_TEST(test_too_long_path_is_rejected);
    return UNITY_END();
}
