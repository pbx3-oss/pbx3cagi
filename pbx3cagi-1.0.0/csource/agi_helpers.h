/*
 * Pure helpers for pbx3cagi (unit-testable; no AGI I/O).
 * Buffer sizes match pbx3cagi.h.
 */
#ifndef AGI_HELPERS_H
#define AGI_HELPERS_H

#include <stddef.h>

#ifndef AGI_HELPERS_MAX_EXT_LEN
#define AGI_HELPERS_MAX_EXT_LEN 64
#endif
#ifndef AGI_HELPERS_MAX_NUM_LEN
#define AGI_HELPERS_MAX_NUM_LEN 32
#endif
#ifndef AGI_HELPERS_MAX_TRANSFORM_LEN
#define AGI_HELPERS_MAX_TRANSFORM_LEN 65
#endif
#ifndef AGI_HELPERS_MAX_NUM_TRANSFORM
#define AGI_HELPERS_MAX_NUM_TRANSFORM 128
#endif

/** Strip leading 4 chars of a feature code (*NN*) into out. */
void get_ext_digits(char *out, size_t outsz, const char *number);

/**
 * Apply trunk transform list (space-separated left:right) to operand_in.
 * transformList is mutated (strtok). Result written to out.
 * preSel may be NULL or empty.
 */
void mangle_number(char *out, size_t outsz, const char *operand_in,
                   const char *preSel, char *transformList);

/**
 * True for NULL, empty, or stock defaults 4444 / 3333 (sys/spy).
 * Callers must fail closed — do not Authenticate with these.
 */
int is_insecure_feature_pass(const char *password_plain);

#endif /* AGI_HELPERS_H */
