#ifndef PROF_FUNC_H
#define PROF_FUNC_H

#include "php.h"
#include "zend_observer.h"

zend_result prof_func_init();
zend_result prof_func_setup();
zend_result prof_func_teardown();
void prof_func_print_result();

#endif //PROF_FUNC_H
