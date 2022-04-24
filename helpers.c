#include "helpers.h"
#include "php_prof.h"
#include "zend_smart_str.h"
#include "SAPI.h"

#include <time.h>

zend_ulong get_time() {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
        return 0;
    }

    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

uint8_t get_prof_mode() {
    if (strcmp(sapi_module.name, "cli") != 0) {
        return PROF_MODE_NONE;
    }

    const char *mode = getenv("PROF_MODE");

    if (!mode) {
        return PROF_MODE_NONE;
    }

    if (strcmp(mode, "sampling") == 0) {
        return PROF_MODE_SAMPLING;
    } else if (strcmp(mode, "func") == 0) {
        return PROF_MODE_FUNC;
    } else if (strcmp(mode, "opcode") == 0) {
        return PROF_MODE_OPCODE;
    }

    return PROF_MODE_NONE;
}

zend_string *get_function_name(zend_function *func) {
    if (func->common.fn_flags & ZEND_ACC_CLOSURE && ZEND_USER_CODE(func->type)) {
        smart_str closure_name = {0};
        smart_str_append_printf(&closure_name, "closure %s:%d", ZSTR_VAL(func->op_array.filename), func->op_array.line_start);
        return smart_str_extract(&closure_name);
    }

    return get_function_or_method_name(func);
}

zend_always_inline int prof_compare_reverse_numeric_unstable_i(Bucket *f, Bucket *s) {
    return -numeric_compare_function(&f->val, &s->val);
}

uint16_t get_prof_key_column_length(HashTable *profile) {
    zend_string *key;
    uint16_t max_function_name_length = 0;

    ZEND_HASH_FOREACH_STR_KEY(profile, key) {
        if (ZSTR_LEN(key) > max_function_name_length) {
            max_function_name_length = ZSTR_LEN(key);
        }
    } ZEND_HASH_FOREACH_END();

    return MAX(MIN_KEY_COLUMN_LENGTH, MIN(max_function_name_length, MAX_KEY_COLUMN_LENGTH));
}
