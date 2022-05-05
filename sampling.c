#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"

#include <time.h>
#include <pthread.h>
#include <signal.h>

#define SAMPLING_HITS_DEFAULT_CAPACITY 4096
#define SAMPLING_TICKS_THRESHOLD 10
#define SAMPLES_LIMIT 20
#define SAMPLING_THRESHOLD 1

ZEND_EXTERN_MODULE_GLOBALS(prof)

void prof_sigprof_handler(int signo);
void *prof_ticker(void *args);

zend_result prof_sampling_init() {
    return SUCCESS;
}

zend_result prof_sampling_setup() {
    PROF_G(sampling_enabled) = true;
    zend_hash_init(&PROF_G(sampling_hits), SAMPLING_HITS_DEFAULT_CAPACITY, NULL, NULL, 0);
    PROF_G(sampling_ticks) = ATOMIC_VAR_INIT(0);

    zend_signal(SIGPROF, prof_sigprof_handler);

    srand(time(NULL));
    PROF_G(sampling_interval) = 1000 + rand() % 500; // todo configurable
    struct itimerval timeout;
    timeout.it_value.tv_sec = timeout.it_interval.tv_sec = 0;
    timeout.it_value.tv_usec = timeout.it_interval.tv_usec = PROF_G(sampling_interval);

    if (setitimer(ITIMER_PROF, &timeout, NULL)) {
        return FAILURE;
    }

    if (pthread_create(&PROF_G(sampling_thread), NULL, prof_ticker, NULL)) {
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

    if (pthread_join(PROF_G(sampling_thread), NULL)) {
        return FAILURE;
    }

    zend_hash_destroy(&PROF_G(sampling_hits));

    return SUCCESS;
}

void prof_sampling_print_result() {
    zend_string *function_name;
    zval *samples;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(sampling_hits));
    zend_ulong total_samples = 0;

    ZEND_HASH_FOREACH_VAL(&PROF_G(sampling_hits), samples) {
        total_samples += Z_LVAL_P(samples);
    } ZEND_HASH_FOREACH_END();

    php_printf("total samples: %ld\n", total_samples);
    if (total_samples == 0) {
        return;
    }
    php_printf("%-*s hits\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(sampling_hits), prof_compare_reverse_numeric_unstable_i, 0);
    HashTable *hits = ht_slice(&PROF_G(sampling_hits), SAMPLES_LIMIT); // todo configurable

    ZEND_HASH_FOREACH_STR_KEY_VAL(hits, function_name, samples) {
        if (Z_LVAL_P(samples) <= SAMPLING_THRESHOLD) { // todo configurable
            continue;
        }

        php_printf("%-*s %ld (%d%%)\n", function_name_column_length, ZSTR_VAL(function_name), Z_LVAL_P(samples),
                   (int)((float)Z_LVAL_P(samples) / total_samples * 100));
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(hits);
    efree(hits);
}

void prof_sigprof_handler(int signo) {
    if (!PROF_G(sampling_enabled)) {
        return;
    }

    uint ticks = atomic_exchange(&PROF_G(sampling_ticks), 0);
    ticks = MAX(1, ticks);

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
    if (ZVAL_IS_NULL(timing)) {
        ZVAL_LONG(timing, ticks);
    } else {
        Z_LVAL_P(timing) += ticks;
    }

    zend_string_release(function_name);
}

void *prof_ticker(void *args) {
    // block SIGPROF in this thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPROF);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL)) {
        PROF_G(error) = true;
        return NULL;
    }

    while (PROF_G(sampling_enabled)) {
        PROF_G(sampling_ticks)++;

        if (PROF_G(sampling_ticks) > SAMPLING_TICKS_THRESHOLD) {
            // probably sleeping
            prof_sigprof_handler(SIGPROF); // possible segfault
        }

        usleep(PROF_G(sampling_interval));
    }

    return NULL;
}
