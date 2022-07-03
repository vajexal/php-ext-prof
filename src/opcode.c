#include "opcode.h"
#include "php_prof.h"
#include "helpers.h"

#include "fort.h"

#define OPCODE_TIMINGS_DEFAULT_CAPACITY 1024
#define OPCODE_LINES_DEFAULT_CAPACITY 128

ZEND_EXTERN_MODULE_GLOBALS(prof)

static int prof_opcode_handler(zend_execute_data *execute_data);
static void prof_opcode_handler_count_timings(zend_execute_data *execute_data);

zend_result prof_opcode_init() {
    // todo handle some subset of opcodes
    for (uint16_t i = 0; i < 256; i++) {
        zend_set_user_opcode_handler(i, prof_opcode_handler);
    }

    return SUCCESS;
}

zend_result prof_opcode_setup() {
    zend_hash_init(&PROF_G(opcode_timings), OPCODE_TIMINGS_DEFAULT_CAPACITY, NULL, NULL, 0);

    return SUCCESS;
}

zend_result prof_opcode_teardown() {
    zval *func_timings;

    ZEND_HASH_FOREACH_VAL(&PROF_G(opcode_timings), func_timings) {
        zend_array_destroy(Z_ARR_P(func_timings));
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&PROF_G(opcode_timings));

    return SUCCESS;
}

void prof_opcode_print_result() {
    zend_string *filename;
    zval *func_timings, *timing;

    ft_set_memory_funcs(local_malloc, local_free);
    ft_table_t *table = ft_create_table();
    ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
    ft_set_border_style(table, FT_SOLID_ROUND_STYLE);

    ft_write_ln(table, "opcode", "time");

    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(opcode_timings), filename, func_timings) {
        uint32_t max_line = zend_hash_next_free_element(Z_ARR_P(func_timings));
        for (uint32_t line = 0; line < max_line; line++) {
            timing = zend_hash_index_find(Z_ARR_P(func_timings), line);
            if (!timing || ZVAL_IS_NULL(timing) || Z_DVAL_P(timing) == PROF_OPCODE_EMPTY_TIMING) {
                continue;
            }
            if (Z_DVAL_P(timing) <= PROF_G(config).opcode_threshold) {
                continue;
            }

            ft_printf_ln(table, "%s:%d|%.6fs", ZSTR_VAL(filename), line, Z_DVAL_P(timing));
        }
    } ZEND_HASH_FOREACH_END();

    php_printf("%s\n", ft_to_string(table));

    ft_destroy_table(table);
}

static int prof_opcode_handler(zend_execute_data *execute_data) {
    prof_opcode_handler_count_timings(execute_data);

    return ZEND_USER_OPCODE_DISPATCH;
}

static void prof_opcode_handler_count_timings(zend_execute_data *execute_data) {
    zend_string *filename = execute_data->func->op_array.filename;
    uint32_t lineno = execute_data->opline->lineno;

    if (PROF_G(opcode_last_file) == NULL && PROF_G(opcode_last_lineno) == 0) {
        PROF_G(opcode_last_file) = filename;
        PROF_G(opcode_last_lineno) = lineno;
        PROF_G(opcode_last_time) = get_time();
        if (!PROF_G(opcode_last_time)) {
            prof_add_warning("get time");
        }
        return;
    }

    if (filename == PROF_G(opcode_last_file) && lineno == PROF_G(opcode_last_lineno)) {
        return;
    }

    zend_ulong end_time = get_time();
    if (!end_time) {
        prof_add_warning("get time");
        return;
    }

    zval *file_timings = zend_hash_lookup(&PROF_G(opcode_timings), filename);
    if (ZVAL_IS_NULL(file_timings)) {
        array_init_size(file_timings, OPCODE_LINES_DEFAULT_CAPACITY);
    }

    zval opline_time;
    ZVAL_DOUBLE(&opline_time, (double)(end_time - PROF_G(opcode_last_time)) / 1000000);
    zend_hash_index_update(Z_ARR_P(file_timings), PROF_G(opcode_last_lineno), &opline_time);

    PROF_G(opcode_last_file) = filename;
    PROF_G(opcode_last_lineno) = lineno;
    PROF_G(opcode_last_time) = end_time;
}
