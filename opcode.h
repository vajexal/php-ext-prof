#ifndef PROF_OPCODE_H
#define PROF_OPCODE_H

#include "php.h"

#define PROF_OPCODE_EMPTY_TIMING -1

zend_result prof_opcode_init();
zend_result prof_opcode_setup();
zend_result prof_opcode_teardown();
void prof_opcode_print_result();

static int prof_opcode_handler(zend_execute_data *execute_data);
static void prof_opcode_handler_count_timings(zend_execute_data *execute_data);

#endif //PROF_OPCODE_H
