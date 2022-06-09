#ifndef PROF_OPCODE_H
#define PROF_OPCODE_H

#include "php.h"

#define PROF_OPCODE_EMPTY_TIMING -1

zend_result prof_opcode_init();
zend_result prof_opcode_setup();
zend_result prof_opcode_teardown();
void prof_opcode_print_result();

#endif //PROF_OPCODE_H
