#include "wd_form.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect_int(const char *name, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        g_fail++;
    }
}

static void expect_str(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s: got '%s' want '%s'\n", name, got, want);
        g_fail++;
    }
}

int main(void)
{
    char out[64];
    int rc;

    expect_int("decode plain", wd_url_decode(out, sizeof(out), "abc", 3), 0);
    expect_str("plain", out, "abc");

    expect_int("decode plus", wd_url_decode(out, sizeof(out), "a+b", 3), 0);
    expect_str("plus", out, "a b");

    expect_int("decode pct", wd_url_decode(out, sizeof(out), "a%2Fb", 5), 0);
    expect_str("pct", out, "a/b");

    const char *form =
        "wifi_ssid=HomeNet&mqtt_uri=mqtt%3A%2F%2F1.2.3.4%3A1883&device_id=cyd1";
    size_t form_len = strlen(form);

    rc = wd_form_get_field(form, form_len, "wifi_ssid", out, sizeof(out));
    expect_int("field ssid rc", rc, 0);
    expect_str("field ssid", out, "HomeNet");

    rc = wd_form_get_field(form, form_len, "mqtt_uri", out, sizeof(out));
    expect_int("field mqtt rc", rc, 0);
    expect_str("field mqtt", out, "mqtt://1.2.3.4:1883");

    rc = wd_form_get_field(form, form_len, "device_id", out, sizeof(out));
    expect_int("field id rc", rc, 0);
    expect_str("field id", out, "cyd1");

    rc = wd_form_get_field("a=1", 3, "missing", out, sizeof(out));
    expect_int("missing rc", rc, 1);

    if (g_fail) {
        fprintf(stderr, "%d failures\n", g_fail);
        return 1;
    }
    printf("== test_wd_form ==\ntotal asserts: ok\n");
    return 0;
}
