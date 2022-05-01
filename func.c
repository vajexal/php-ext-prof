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
    zend_stack_init(&PROF_G(func_start_times), sizeof(zend_ulong));

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

    php_printf("%-*s time        calls\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(func_timings), prof_compare_func_timings, 0);
    HashTable *timings = ht_slice(&PROF_G(func_timings), FUNC_TIMINGS_LIMIT); // todo configurable

    prof_func_entry *entry;
    ZEND_HASH_FOREACH_STR_KEY_PTR(timings, function_name, entry) {
        php_printf("%-*s %.6fs   %d\n", function_name_column_length, ZSTR_VAL(function_name), entry->time, entry->calls);
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
    zend_ulong start_time = get_time();
    if (!start_time) {
        PROF_G(error) = true;
        return;
    }

    zend_stack_push(&PROF_G(func_start_times), &start_time);
}

static void prof_observer_end(zend_execute_data *execute_data, zval *retval) {
    zend_ulong end_time = get_time();
    if (!end_time) {
        PROF_G(error) = true;
        return;
    }

    zend_string *function_name = get_function_name(execute_data->func);
    if (!function_name) {
        PROF_G(error) = true;
        return;
    }

    zend_ulong *start_time = zend_stack_top(&PROF_G(func_start_times));
    if (!start_time) {
        zend_string_release(function_name);
        PROF_G(error) = true;
        return;
    }
    zend_stack_del_top(&PROF_G(func_start_times));

    double function_time = (double)(end_time - (*start_time)) / 1000000;

    prof_func_entry *entry = zend_hash_find_ptr(&PROF_G(func_timings), function_name);
    if (!entry) {
        entry = emalloc(sizeof(prof_func_entry));
        memset(entry, 0, sizeof(prof_func_entry));
        zend_hash_add_new_ptr(&PROF_G(func_timings), function_name, entry);
    }

    entry->time += function_time;
    entry->calls++;

    zend_string_release(function_name);
}

static zend_always_inline int prof_compare_func_timings(Bucket *f, Bucket *s) {
    prof_func_entry *entry_f = (prof_func_entry*)Z_PTR(f->val);
    prof_func_entry *entry_s = (prof_func_entry*)Z_PTR(s->val);

    return -ZEND_NORMALIZE_BOOL(entry_f->time - entry_s->time);
}
