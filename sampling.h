#ifndef PROF_SAMPLING_H
#define PROF_SAMPLING_H

#include "php.h"

zend_result prof_sampling_init();
zend_result prof_sampling_setup();
zend_result prof_sampling_teardown();
void prof_sampling_print_result();

#endif //PROF_SAMPLING_H
