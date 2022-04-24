#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"

#include <pthread.h>
#include <time.h>

#define PROF_SAMPLING_INTERVAL 1000 + rand() % 500

ZEND_EXTERN_MODULE_GLOBALS(prof)

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
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(sampling_hits));

    php_printf("Profiling results\n\n");
    php_printf("%-*s hits\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(sampling_hits), prof_compare_reverse_numeric_unstable_i, 0);

    zval *hits;
    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(sampling_hits), function_name, hits) {
        php_printf("%-*s %ld\n", function_name_column_length, ZSTR_VAL(function_name), Z_LVAL_P(hits));
    } ZEND_HASH_FOREACH_END();
}

static void *prof_sample_handler(void *args) {
    srand(time(NULL));

    while (PROF_G(sampling_enabled)) {
        volatile zend_execute_data *execute_data = EG(current_execute_data);

        if (!execute_data || !execute_data->func || !ZEND_USER_CODE(execute_data->func->type)) {
            usleep(PROF_SAMPLING_INTERVAL);
            continue;
        }

        zend_string *function_name = get_function_name(execute_data->func);
        zval *timing = zend_hash_lookup(&PROF_G(sampling_hits), function_name);
        increment_function(timing);
        zend_string_release(function_name);

        usleep(PROF_SAMPLING_INTERVAL);
    }

    return NULL;
}
