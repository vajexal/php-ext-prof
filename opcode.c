#include "opcode.h"
#include "php_prof.h"
#include "helpers.h"

ZEND_EXTERN_MODULE_GLOBALS(prof)

zend_result prof_opcode_init() {
    // todo handle some subset of opcodes
    for (uint16_t i = 0; i < 256; i++) {
        zend_set_user_opcode_handler(i, prof_opcode_handler);
    }

    return SUCCESS;
}

zend_result prof_opcode_setup() {
    zend_hash_init(&PROF_G(opcode_timings), HT_MIN_SIZE, NULL, NULL, 0); // todo

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
    zval *func_timings;
    zval *timing;
    uint16_t line_column_length = get_prof_key_column_length(&PROF_G(opcode_timings));
    char line_name[1024];

    // for line num after filename in column...
    if (line_column_length + 5 <= MAX_KEY_COLUMN_LENGTH) {
        line_column_length += 5;
    }

    php_printf("Profiling results\n\n");
    php_printf("%-*s time\n", line_column_length, "line");

    ZEND_HASH_FOREACH_STR_KEY_VAL(&PROF_G(opcode_timings), filename, func_timings) {
        uint32_t max_line = zend_hash_next_free_element(Z_ARR_P(func_timings));
        for (uint32_t line = 0; line < max_line; line++) {
            timing = zend_hash_index_find(Z_ARR_P(func_timings), line);
            if (!timing || ZVAL_IS_NULL(timing) || Z_DVAL_P(timing) == PROF_OPCODE_EMPTY_TIMING) {
                continue;
            }
            sprintf(line_name, "%s:%d", ZSTR_VAL(filename), line);
            php_printf("%-*s %.6fs\n", line_column_length, line_name, Z_DVAL_P(timing));
            line_name[0] = '\0';
        }
    } ZEND_HASH_FOREACH_END();
}

static int prof_opcode_handler(zend_execute_data *execute_data) {
    prof_opcode_handler_count_timings(execute_data);

    return ZEND_USER_OPCODE_DISPATCH;
}

static void prof_opcode_handler_count_timings(zend_execute_data *execute_data) {
    zval *file_timings = zend_hash_lookup(&PROF_G(opcode_timings), execute_data->func->op_array.filename);

    if (ZVAL_IS_NULL(file_timings)) {
        array_init(file_timings);

        // fill first opcode of the file
        zend_ulong opcode_start_time = get_time();
        if (!opcode_start_time) {
            PROF_G(error) = true;
            return;
        }
        PROF_G(opcode_start_time) = opcode_start_time;

        zval opline_time;
        ZVAL_DOUBLE(&opline_time, PROF_OPCODE_EMPTY_TIMING);
        zend_hash_index_update(Z_ARR_P(file_timings), execute_data->opline->lineno, &opline_time);

        return;
    }

    zval *timing = zend_hash_index_lookup(Z_ARR_P(file_timings), execute_data->opline->lineno);
    if (ZVAL_IS_NULL(timing)) {
        // if we moved to the next line, then count timing for prev line
        zend_ulong end_time = get_time();
        if (!end_time) {
            PROF_G(error) = true;
            return;
        }

        zval opline_time;
        ZVAL_DOUBLE(&opline_time, (double)(end_time - PROF_G(opcode_start_time)) / 1000000);
        zend_hash_index_update(Z_ARR_P(file_timings), execute_data->opline->lineno - 1, &opline_time);

        zval opline_time2;
        ZVAL_DOUBLE(&opline_time2, PROF_OPCODE_EMPTY_TIMING);
        zend_hash_index_update(Z_ARR_P(file_timings), execute_data->opline->lineno, &opline_time2);

        // todo packed array stat

        PROF_G(opcode_start_time) = end_time;
    }
}
