#include "func.h"
#include "php_prof.h"
#include "helpers.h"

#define FUNC_TIMINGS_DEFAULT_CAPACITY 4096
#define FUNC_TIMINGS_LIMIT 50

ZEND_EXTERN_MODULE_GLOBALS(prof)

static zend_observer_fcall_handlers prof_observer(zend_execute_data *execute_data);
static void prof_observer_begin(zend_execute_data *execute_data);
static void prof_observer_end(zend_execute_data *execute_data, zval *retval);
static zend_always_inline int prof_compare_func_timings(Bucket *f, Bucket *s);

zend_result prof_func_init() {
    zend_observer_fcall_register(prof_observer);

    return SUCCESS;
}

zend_result prof_func_setup() {
    zend_hash_init(&PROF_G(func_timings), FUNC_TIMINGS_DEFAULT_CAPACITY, NULL, NULL, 0);
    zend_stack_init(&PROF_G(func_start_times), sizeof(prof_timing));

    return SUCCESS;
}

zend_result prof_func_teardown() {
    prof_func_entry *entry;
    ZEND_HASH_FOREACH_PTR(&PROF_G(func_timings), entry) {
        efree(entry);
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&PROF_G(func_timings));
    zend_stack_destroy(&PROF_G(func_start_times));

    return SUCCESS;
}

void prof_func_print_result() {
    zend_string *function_name;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(func_timings));

    php_printf("%-*s wall        cpu         memory     calls\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(func_timings), prof_compare_func_timings, 0);
    HashTable *timings = ht_slice(&PROF_G(func_timings), FUNC_TIMINGS_LIMIT); // todo configurable

    prof_func_entry *entry;
    char memory_buf[80];
    ZEND_HASH_FOREACH_STR_KEY_PTR(timings, function_name, entry) {
        memset(memory_buf, 0, sizeof(memory_buf));
        get_memory_with_units(entry->memory, memory_buf, sizeof(memory_buf));
        php_printf("%-*s %.6fs   %.6fs   %-10s %d\n",
                   function_name_column_length, ZSTR_VAL(function_name), entry->wall_time, entry->cpu_time, memory_buf, entry->calls
        );
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(timings);
    efree(timings);
}

static zend_observer_fcall_handlers prof_observer(zend_execute_data *execute_data) {
    zend_observer_fcall_handlers handlers = {};

    if (!execute_data->func || !execute_data->func->common.function_name) {
        return handlers;
    }

    handlers.begin = prof_observer_begin;
    handlers.end = prof_observer_end;

    return handlers;
}

static void prof_observer_begin(zend_execute_data *execute_data) {
    prof_timing *timing = get_timing();
    if (!timing) {
        PROF_G(error) = true;
        return;
    }

    zend_stack_push(&PROF_G(func_start_times), timing);
    efree(timing);
}

static void prof_observer_end(zend_execute_data *execute_data, zval *retval) {
    prof_timing *end_timing = get_timing();
    if (!end_timing) {
        PROF_G(error) = true;
        return;
    }

    zend_string *function_name = get_function_name(execute_data->func);
    if (!function_name) {
        efree(end_timing);
        PROF_G(error) = true;
        return;
    }

    prof_timing *timing = zend_stack_top(&PROF_G(func_start_times));
    if (!timing) {
        efree(end_timing);
        zend_string_release(function_name);
        PROF_G(error) = true;
        return;
    }
    zend_stack_del_top(&PROF_G(func_start_times));

    prof_func_entry *entry = zend_hash_find_ptr(&PROF_G(func_timings), function_name);
    if (!entry) {
        entry = emalloc(sizeof(prof_func_entry));
        memset(entry, 0, sizeof(prof_func_entry));
        zend_hash_add_new_ptr(&PROF_G(func_timings), function_name, entry);
    }

    entry->wall_time += (double)(end_timing->wall - (timing->wall)) / 1000000;
    entry->cpu_time += (double)(end_timing->cpu - (timing->cpu)) / 1000000;
    entry->memory += end_timing->memory - timing->memory;
    entry->calls++;

    efree(end_timing);
    zend_string_release(function_name);
}

static zend_always_inline int prof_compare_func_timings(Bucket *f, Bucket *s) {
    prof_func_entry *entry_f = (prof_func_entry *)Z_PTR(f->val);
    prof_func_entry *entry_s = (prof_func_entry *)Z_PTR(s->val);

    return -ZEND_NORMALIZE_BOOL(entry_f->wall_time - entry_s->wall_time);
}
