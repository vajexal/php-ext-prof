#include "func.h"
#include "php_prof.h"
#include "helpers.h"

#define FUNC_UNITS_DEFAULT_CAPACITY 4096
#define FUNC_UNITS_LIMIT 50
#define FUNC_THRESHOLD 0.000001

ZEND_EXTERN_MODULE_GLOBALS(prof)

static zend_observer_fcall_handlers prof_observer(zend_execute_data *execute_data);
static void prof_observer_begin(zend_execute_data *execute_data);
static void prof_observer_end(zend_execute_data *execute_data, zval *retval);
static zend_always_inline int prof_compare_func_units(Bucket *f, Bucket *s);

zend_result prof_func_init() {
    zend_observer_fcall_register(prof_observer);

    return SUCCESS;
}

zend_result prof_func_setup() {
    zend_hash_init(&PROF_G(func_units), FUNC_UNITS_DEFAULT_CAPACITY, NULL, NULL, 0);
    zend_stack_init(&PROF_G(func_start_units), sizeof(prof_unit));

    return SUCCESS;
}

zend_result prof_func_teardown() {
    prof_func_unit *func_unit;
    ZEND_HASH_FOREACH_PTR(&PROF_G(func_units), func_unit) {
        efree(func_unit);
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&PROF_G(func_units));
    zend_stack_destroy(&PROF_G(func_start_units));

    return SUCCESS;
}

void prof_func_print_result() {
    zend_string *function_name;
    uint16_t function_name_column_length = get_prof_key_column_length(&PROF_G(func_units));

    php_printf("%-*s wall        cpu         memory     calls\n", function_name_column_length, "function");

    zend_hash_sort(&PROF_G(func_units), prof_compare_func_units, 0);
    HashTable *func_units = ht_slice(&PROF_G(func_units), FUNC_UNITS_LIMIT); // todo configurable

    prof_func_unit *func_unit;
    char memory_buf[80];
    ZEND_HASH_FOREACH_STR_KEY_PTR(func_units, function_name, func_unit) {
        if (func_unit->wall_time <= FUNC_THRESHOLD) { // todo configurable
            continue;
        }

        memset(memory_buf, 0, sizeof(memory_buf));
        get_memory_with_units(func_unit->memory, memory_buf, sizeof(memory_buf));
        php_printf("%-*s %.6fs   %.6fs   %-10s %d\n",
                   function_name_column_length, ZSTR_VAL(function_name), func_unit->wall_time, func_unit->cpu_time, memory_buf, func_unit->calls
        );
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(func_units);
    efree(func_units);
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
    prof_unit *unit = get_prof_unit();
    if (!unit) {
        PROF_G(error) = true;
        return;
    }

    zend_stack_push(&PROF_G(func_start_units), unit);
    efree(unit);
}

static void prof_observer_end(zend_execute_data *execute_data, zval *retval) {
    prof_unit *end_unit = get_prof_unit();
    if (!end_unit) {
        PROF_G(error) = true;
        return;
    }

    zend_string *function_name = get_function_name(execute_data->func);
    if (!function_name) {
        efree(end_unit);
        PROF_G(error) = true;
        return;
    }

    prof_unit *unit = zend_stack_top(&PROF_G(func_start_units));
    if (!unit) {
        efree(end_unit);
        zend_string_release(function_name);
        PROF_G(error) = true;
        return;
    }
    zend_stack_del_top(&PROF_G(func_start_units));

    prof_func_unit *func_unit = zend_hash_find_ptr(&PROF_G(func_units), function_name);
    if (!func_unit) {
        func_unit = emalloc(sizeof(prof_func_unit));
        memset(func_unit, 0, sizeof(prof_func_unit));
        zend_hash_add_new_ptr(&PROF_G(func_units), function_name, func_unit);
    }

    func_unit->wall_time += (double)(end_unit->wall - (unit->wall)) / 1000000;
    func_unit->cpu_time += (double)(end_unit->cpu - (unit->cpu)) / 1000000;
    func_unit->memory += end_unit->memory - unit->memory;
    func_unit->calls++;

    efree(end_unit);
    zend_string_release(function_name);
}

static zend_always_inline int prof_compare_func_units(Bucket *f, Bucket *s) {
    prof_func_unit *func_unit_f = (prof_func_unit *)Z_PTR(f->val);
    prof_func_unit *func_unit_s = (prof_func_unit *)Z_PTR(s->val);

    return -ZEND_NORMALIZE_BOOL(func_unit_f->wall_time - func_unit_s->wall_time);
}
