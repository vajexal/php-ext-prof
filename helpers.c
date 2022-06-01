#include "helpers.h"
#include "php_prof.h"
#include "zend_smart_str.h"
#include "SAPI.h"

#include <time.h>

ZEND_EXTERN_MODULE_GLOBALS(prof)

prof_unit *get_prof_unit() {
    prof_unit *unit = emalloc(sizeof(prof_unit));
    if (!unit) {
        return NULL;
    }

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
        efree(unit);
        return NULL;
    }
    unit->wall = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts)) {
        efree(unit);
        return NULL;
    }
    unit->cpu = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

    unit->memory = zend_memory_usage(false);

    return unit;
}

zend_ulong get_time() {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
        return 0;
    }

    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

zend_ulong get_wall_time() {
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        return 0;
    }

    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

prof_mode get_prof_mode() {
    if (sapi_module.name && strcmp(sapi_module.name, "cli") != 0) {
        return PROF_MODE_NONE;
    }

    const char *ini_mode = INI_STR("prof.mode");
    const char *mode = ini_mode && ini_mode[0] ? ini_mode : getenv("PROF_MODE");

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

prof_output_mode get_prof_output_mode() {
    const char *ini_mode = INI_STR("prof.output_mode");
    const char *mode = ini_mode && ini_mode[0] ? ini_mode : getenv("PROF_OUTPUT_MODE");

    if (!mode) {
        return PROF_OUTPUT_MODE_CONSOLE;
    }

    if (strcmp(mode, "callgrind") == 0) {
        return PROF_OUTPUT_MODE_CALLGRIND;
    } else if (strcmp(mode, "pprof") == 0) {
        return PROF_OUTPUT_MODE_PPROF;
    }

    return PROF_OUTPUT_MODE_CONSOLE;
}

zend_string *get_function_name(zend_function *func) {
    if ((func->common.fn_flags & ZEND_ACC_CLOSURE) && ZEND_USER_CODE(func->type)) {
        smart_str closure_name = {0};
        smart_str_append_printf(&closure_name, "closure %s:%d", ZSTR_VAL(func->op_array.filename), func->op_array.line_start);
        return smart_str_extract(&closure_name);
    }

    // could be 'require' from class method
    if (func->common.scope && !func->common.function_name) {
        return zend_string_init("main", sizeof("main") - 1, 0);
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

void prof_print_common_header() {
    zend_ulong end_time = get_wall_time();
    if (!end_time) {
        return;
    }

    double total_time = (double)(end_time - PROF_G(start_time)) / 1000000000;

    php_printf("total time: %.6fs\n\n", total_time);
}

HashTable *ht_slice(HashTable *ht, zend_ulong limit) {
    zend_ulong n = 0;  /* Current number of elements */
    zval *entry;
    HashTable *res = emalloc(sizeof(HashTable));
    zend_hash_init(res, limit, NULL, NULL, 0);
    Bucket *p = ht->arData;
    Bucket *end = ht->arData + ht->nNumUsed;

    for (; p != end; p++) {
        entry = &p->val;
        if (UNEXPECTED(Z_TYPE_P(entry) == IS_UNDEF)) {
            continue;
        }
        if (n >= limit) {
            break;
        }
        n++;

        entry = zend_hash_add_new(res, p->key, entry);
        zval_add_ref(entry);
    }

    return res;
}

void get_memory_with_units(zend_long memory, char *buf, size_t buf_len) {
    memory = ZEND_ABS(memory);

    if (memory < 1024) {
        snprintf(buf, buf_len, "%ld b", memory);
    } else if (memory < 1024 * 1024) {
        snprintf(buf, buf_len, "%.2f kb", (float)memory / 1024);
    } else {
        snprintf(buf, buf_len, "%.2f mb", (float)memory / 1024 / 1024);
    }
}

#if PHP_VERSION_ID < 80100
zval* ZEND_FASTCALL zend_hash_lookup(HashTable *ht, zend_string *key) {
    zval *zv = zend_hash_find(ht, key);
    if (zv) {
        return zv;
    }

    return zend_hash_add_empty_element(ht, key);
}
#endif
