#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"

#include <php_main.h>
#include <zend_smart_string.h>

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
static zend_always_inline int prof_compare_sampling_units(Bucket *f, Bucket *s);

zend_result prof_sampling_init() {
    return SUCCESS;
}

zend_result prof_sampling_setup() {
    PROF_G(sampling_enabled) = true;
    zend_hash_init(&PROF_G(sampling_units), SAMPLING_HITS_DEFAULT_CAPACITY, NULL, NULL, 0);
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

    prof_sampling_unit *sampling_unit;
    ZEND_HASH_FOREACH_PTR(&PROF_G(sampling_units), sampling_unit) {
        zend_string_release(sampling_unit->filename);
        efree(sampling_unit);
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&PROF_G(sampling_units));

    return SUCCESS;
}

void prof_sampling_print_result_console() {
    zend_string *function_name;
    prof_sampling_unit *sampling_unit;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(sampling_units));
    zend_ulong total_samples = 0;

    ZEND_HASH_FOREACH_PTR(&PROF_G(sampling_units), sampling_unit) {
        total_samples += sampling_unit->hits;
    } ZEND_HASH_FOREACH_END();

    php_printf("total samples: %ld\n", total_samples);
    if (total_samples == 0) {
        return;
    }
    php_printf("%-*s hits\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(sampling_units), prof_compare_sampling_units, 0);
    HashTable *hits = ht_slice(&PROF_G(sampling_units), SAMPLES_LIMIT); // todo configurable

    ZEND_HASH_FOREACH_STR_KEY_PTR(hits, function_name, sampling_unit) {
        if (sampling_unit->hits <= SAMPLING_THRESHOLD) { // todo configurable
            continue;
        }

        php_printf("%-*s %ld (%d%%)\n", function_name_column_length, ZSTR_VAL(function_name), sampling_unit->hits,
                   (int)((float)sampling_unit->hits / total_samples * 100));
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(hits);
    efree(hits);
}

void prof_sampling_print_result_callgrind() {
    char filename_buf[80];
    snprintf(filename_buf, sizeof(filename_buf), "callgrind.out.%d", getpid());

    FILE *fp = fopen(filename_buf, "w");
    if (!fp) {
        return;
    }

    smart_string cmd = {0};
    for (int i = 0; i < SG(request_info).argc; ++i) {
        smart_string_appends(&cmd, SG(request_info).argv[i]);
        if (i + 1 < SG(request_info).argc) {
            smart_string_appendc(&cmd, ' ');
        }
    }
    smart_string_0(&cmd);

    fprintf(fp, "# callgrind format\nversion: 1\ncmd: %s\npart: 1\n\nevents: Hits\n\n", cmd.c);

    // todo summary

    zend_string *function_name;
    prof_sampling_unit *sampling_unit;
    ZEND_HASH_FOREACH_STR_KEY_PTR(&PROF_G(sampling_units), function_name, sampling_unit) {
        if (sampling_unit->hits <= SAMPLING_THRESHOLD) { // todo configurable
            continue;
        }

        // todo name compression
        fprintf(fp, "fl=%s\nfn=%s\n%d %lu\n\n", ZSTR_VAL(sampling_unit->filename), ZSTR_VAL(function_name), sampling_unit->lineno, sampling_unit->hits);
    } ZEND_HASH_FOREACH_END();

    // todo totals

    smart_string_free(&cmd);
    fclose(fp);
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

    prof_sampling_unit *sampling_unit = zend_hash_find_ptr(&PROF_G(sampling_units), function_name);
    if (!sampling_unit) {
        sampling_unit = emalloc(sizeof(prof_sampling_unit));
        memset(sampling_unit, 0, sizeof(prof_sampling_unit));
        sampling_unit->filename = zend_string_copy(execute_data->func->op_array.filename);
        sampling_unit->lineno = execute_data->func->op_array.line_start;
        zend_hash_add_new_ptr(&PROF_G(sampling_units), function_name, sampling_unit);
    }

    sampling_unit->hits += ticks;

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

static zend_always_inline int prof_compare_sampling_units(Bucket *f, Bucket *s) {
    prof_sampling_unit *sampling_unit_f = (prof_sampling_unit *)Z_PTR(f->val);
    prof_sampling_unit *sampling_unit_s = (prof_sampling_unit *)Z_PTR(s->val);

    return sampling_unit_f->hits == sampling_unit_s->hits ? 0 : (sampling_unit_f->hits > sampling_unit_s->hits ? -1 : 1);
}
