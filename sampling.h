#ifndef PROF_SAMPLING_H
#define PROF_SAMPLING_H

#include "php.h"

typedef struct {
    zend_ulong hits;
    zend_string *filename;
    uint32_t lineno;
} prof_sampling_unit;

zend_result prof_sampling_init();
zend_result prof_sampling_setup();
zend_result prof_sampling_teardown();
void prof_sampling_print_result_console();
void prof_sampling_print_result_callgrind();
void prof_sampling_print_result_pprof();

#endif //PROF_SAMPLING_H
