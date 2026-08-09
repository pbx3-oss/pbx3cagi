#include <stdio.h>
#include <string.h>

#include "agi_helpers.h"

static int failures;

static void expect_streq(const char *name, const char *got, const char *want)
{
    if (strcmp(got ? got : "", want ? want : "") != 0) {
        fprintf(stderr, "FAIL %s: got \"%s\" want \"%s\"\n", name, got ? got : "(null)",
                want ? want : "(null)");
        failures++;
    } else {
        printf("PASS %s\n", name);
    }
}

static void expect_true(const char *name, int cond)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    char out[64];
    char transform[128];

    get_ext_digits(out, sizeof(out), "*68*1101");
    expect_streq("get_ext *68*1101", out, "1101");

    get_ext_digits(out, sizeof(out), "*67*1500");
    expect_streq("get_ext *67*1500", out, "1500");

    get_ext_digits(out, sizeof(out), "*60");
    expect_streq("get_ext short", out, "");

    strcpy(transform, "0:+44 00:+");
    mangle_number(out, sizeof(out), "01924910444", NULL, transform);
    expect_streq("mangle UK 0:+44", out, "+441924910444");

    strcpy(transform, "00:+ 0:+44");
    mangle_number(out, sizeof(out), "00441234567890", NULL, transform);
    expect_streq("mangle UK 00:+ first", out, "+441234567890");

    strcpy(transform, "9:");
    mangle_number(out, sizeof(out), "91234", "9", transform);
    expect_streq("mangle preselect+strip", out, "1234");

    expect_true("insecure NULL", is_insecure_feature_pass(NULL));
    expect_true("insecure empty", is_insecure_feature_pass(""));
    expect_true("insecure 4444", is_insecure_feature_pass("4444"));
    expect_true("insecure 3333", is_insecure_feature_pass("3333"));
    expect_true("secure custom", !is_insecure_feature_pass("9911"));
    expect_true("secure long", !is_insecure_feature_pass("123456"));

    return failures ? 1 : 0;
}
