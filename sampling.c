#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"

#include <pthread.h>
#include <time.h>

#define PROF_SAMPLING_INTERVAL 1000 + rand() % 500

ZEND_EXTERN_MODULE_GLOBALS(prof)

static void *prof_sample_handler(void *args);
static void prof_process_sample(volatile zend_execute_data *execute_data);

zend_result prof_sampling_init() {
    return SUCCESS;
}

zend_result prof_sampling_setup() {
    PROF_G(sampling_enabled) = true;
    zend_hash_init(&PROF_G(sampling_hits), HT_MIN_SIZE, NULL, NULL, 0); // todo size

    if (pthread_create(&PROF_G(sampling_thread), NULL, prof_sample_handler, NULL)) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result prof_sampling_teardown() {
    PROF_G(sampling_enabled) = false;
    if (pthread_join(PROF_G(sampling_thread), NULL)) {
        return FAILURE;
    }

    zend_hash_destroy(&PROF_G(sampling_hits));

    return SUCCESS;
}

void prof_sampling_print_result() {
    zend_string *function_name;
    zval *hits;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(sampling_hits));
    zend_ulong total_samples = 0;

    ZEND_HASH_FOREACH_VAL(&PROF_G(sampling_hits), hits) {
        total_samples += Z_LVAL_P(hits);
    } ZEND_HASH_FOREACH_END();

    php_printf("total samples: %ld\n", total_samples);
    php_printf("%-*s hits\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(sampling_hits), prof_compare_reverse_numeric_unstable_i, 0);

    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(sampling_hits), function_name, hits) {
        php_printf("%-*s %ld (%d%%)\n", function_name_column_length, ZSTR_VAL(function_name), Z_LVAL_P(hits),
                   (int)((float)Z_LVAL_P(hits) / total_samples * 100));
    } ZEND_HASH_FOREACH_END();
}

static void *prof_sample_handler(void *args) {
    srand(time(NULL));

    while (PROF_G(sampling_enabled)) {
        prof_process_sample(EG(current_execute_data));

        usleep(PROF_SAMPLING_INTERVAL);
    }

    return NULL;
}

static void prof_process_sample(volatile zend_execute_data *execute_data) {
    zend_function *func;

    if (!execute_data || !execute_data->func) {
        return;
    }

    if (ZEND_USER_CODE(execute_data->func->type)) {
        func = execute_data->func;
    } else if (execute_data->prev_execute_data && execute_data->prev_execute_data->func && ZEND_USER_CODE(execute_data->prev_execute_data->func->type)) {
        func = execute_data->prev_execute_data->func;
    } else {
        return;
    }

    zend_string *function_name = get_function_name(func);
    zval *timing = zend_hash_lookup(&PROF_G(sampling_hits), function_name);
    increment_function(timing);
    zend_string_release(function_name);
}
