#include <string.h>

#include "agi_helpers.h"
#include "bsd_compat.h"

void get_ext_digits(char *out, size_t outsz, const char *number)
{
    char tmp[AGI_HELPERS_MAX_EXT_LEN];
    size_t i;
    size_t n;

    if (out == NULL || outsz == 0) {
        return;
    }
    out[0] = '\0';
    if (number == NULL) {
        return;
    }

    strlcpy(tmp, number, sizeof(tmp));
    n = strlen(tmp);
    if (n <= 4) {
        return;
    }
    for (i = 0; i < n - 4 && i + 1 < outsz; i++) {
        out[i] = tmp[i + 4];
    }
    if (i >= outsz) {
        i = outsz - 1;
    }
    out[i] = '\0';
}

static void strip_preselect_inplace(const char *preSel, char *number)
{
    size_t plen;
    size_t i;
    size_t n;

    if (preSel == NULL || preSel[0] == '\0' || number == NULL) {
        return;
    }
    plen = strlen(preSel);
    if (strncmp(number, preSel, plen) != 0) {
        return;
    }
    n = strlen(number);
    for (i = 0; i + plen <= n; i++) {
        number[i] = number[i + plen];
    }
}

void mangle_number(char *out, size_t outsz, const char *operand_in,
                   const char *preSel, char *transformList)
{
    char transformArr[AGI_HELPERS_MAX_NUM_TRANSFORM][AGI_HELPERS_MAX_TRANSFORM_LEN];
    char operand[AGI_HELPERS_MAX_NUM_LEN];
    char *transform;
    char *left;
    char *right;
    int i = 0;
    int j;
    int k;
    int len;

    if (out == NULL || outsz == 0) {
        return;
    }
    out[0] = '\0';
    if (operand_in == NULL) {
        return;
    }

    strlcpy(operand, operand_in, sizeof(operand));
    if (preSel != NULL && preSel[0] != '\0') {
        strip_preselect_inplace(preSel, operand);
    }

    if (transformList == NULL || transformList[0] == '\0') {
        strlcpy(out, operand, outsz);
        return;
    }

    transform = strtok(transformList, " ");
    while (transform && i < AGI_HELPERS_MAX_NUM_TRANSFORM) {
        strlcpy(transformArr[i], transform, sizeof(transformArr[i]));
        transform = strtok(NULL, " ");
        i++;
    }

    for (j = 0; j < i; j++) {
        if (transformArr[j][0] == ':') {
            left = "";
            right = strtok(transformArr[j], ":");
        } else {
            left = strtok(transformArr[j], ":");
            right = strtok(NULL, "\0");
        }
        if (left == NULL) {
            left = "";
        }
        if (!strncmp(operand, left, strlen(left))) {
            len = (int)strlen(left);
            for (k = 0; k < (int)strlen(operand); k++) {
                operand[k] = operand[k + len];
            }
            if (right) {
                char mangled[AGI_HELPERS_MAX_NUM_LEN];
                strlcpy(mangled, right, sizeof(mangled));
                strlcat(mangled, operand, sizeof(mangled));
                strlcpy(operand, mangled, sizeof(operand));
            }
        }
    }

    strlcpy(out, operand, outsz);
}

int is_insecure_feature_pass(const char *password_plain)
{
    if (password_plain == NULL || password_plain[0] == '\0') {
        return 1;
    }
    if (!strcmp(password_plain, "4444") || !strcmp(password_plain, "3333")) {
        return 1;
    }
    return 0;
}
