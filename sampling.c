#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"

#include <time.h>

#define SAMPLING_HITS_DEFAULT_CAPACITY 4096
#define PROF_SAMPLING_INTERVAL 1000 + rand() % 500

ZEND_EXTERN_MODULE_GLOBALS(prof)

void prof_sigprof_handler(int signo);

zend_result prof_sampling_init() {
    return SUCCESS;
}

zend_result prof_sampling_setup() {
    PROF_G(sampling_enabled) = true;
    zend_hash_init(&PROF_G(sampling_hits), SAMPLING_HITS_DEFAULT_CAPACITY, NULL, NULL, 0);

    zend_signal(SIGPROF, prof_sigprof_handler);

    srand(time(NULL));
    struct itimerval timeout;
    timeout.it_value.tv_sec = timeout.it_interval.tv_sec = 0;
    timeout.it_value.tv_usec = timeout.it_interval.tv_usec = PROF_SAMPLING_INTERVAL;

    if (setitimer(ITIMER_PROF, &timeout, NULL)) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result prof_sampling_teardown() {
    PROF_G(sampling_enabled) = false;

    struct itimerval no_timeout;
    no_timeout.it_value.tv_sec = no_timeout.it_value.tv_usec = no_timeout.it_interval.tv_sec = no_timeout.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_PROF, &no_timeout, NULL)) {
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
    if (total_samples == 0) {
        return;
    }
    php_printf("%-*s hits\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(sampling_hits), prof_compare_reverse_numeric_unstable_i, 0);

    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(sampling_hits), function_name, hits) {
        php_printf("%-*s %ld (%d%%)\n", function_name_column_length, ZSTR_VAL(function_name), Z_LVAL_P(hits),
                   (int)((float)Z_LVAL_P(hits) / total_samples * 100));
    } ZEND_HASH_FOREACH_END();
}

void prof_sigprof_handler(int signo) {
    if (!PROF_G(sampling_enabled)) {
        return;
    }

    zend_execute_data *execute_data = EG(current_execute_data);

    while (execute_data && (!execute_data->func || !ZEND_USER_CODE(execute_data->func->type))) {
        execute_data = execute_data->prev_execute_data;
    }

    if (!execute_data) {
        return;
    }

    zend_string *function_name = get_function_name(execute_data->func);
    if (!function_name) {
        PROF_G(error) = true;
        return;
    }
    zval *timing = zend_hash_lookup(&PROF_G(sampling_hits), function_name);
    increment_function(timing);
    zend_string_release(function_name);
}
