#include "func.h"
#include "php_prof.h"
#include "helpers.h"

ZEND_EXTERN_MODULE_GLOBALS(prof)

zend_result prof_func_init() {
    zend_observer_fcall_register(prof_observer);

    return SUCCESS;
}

zend_result prof_func_setup() {
    zend_hash_init(&PROF_G(func_timings), HT_MIN_SIZE, NULL, NULL, 0); // todo
    zend_stack_init(&PROF_G(func_start_times), sizeof(zend_ulong));

    return SUCCESS;
}

zend_result prof_func_teardown() {
    zend_hash_destroy(&PROF_G(func_timings));
    zend_stack_destroy(&PROF_G(func_start_times));

    return SUCCESS;
}

void prof_func_print_result() {
    zend_string *function_name;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(func_timings));

    php_printf("Profiling results\n\n");
    php_printf("%-*s time\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(func_timings), prof_compare_reverse_numeric_unstable_i, 0);

    zval *time;
    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(func_timings), function_name, time) {
        php_printf("%-*s %.6fs\n", function_name_column_length, ZSTR_VAL(function_name), Z_DVAL_P(time));
    } ZEND_HASH_FOREACH_END();
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

    zval *timing = zend_hash_lookup(&PROF_G(func_timings), function_name);
    if (ZVAL_IS_NULL(timing)) {
        ZVAL_DOUBLE(timing, function_time);
    } else {
        ZVAL_DOUBLE(timing, Z_DVAL_P(timing) + function_time);
    }

    zend_string_release(function_name);
}
