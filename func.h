#ifndef PROF_FUNC_H
#define PROF_FUNC_H

#include "php.h"
#include "zend_observer.h"

zend_result prof_func_init();
zend_result prof_func_setup();
zend_result prof_func_teardown();
void prof_func_print_result();

static zend_observer_fcall_handlers prof_observer(zend_execute_data *execute_data);
static void prof_observer_begin(zend_execute_data *execute_data);
static void prof_observer_end(zend_execute_data *execute_data, zval *retval);

#endif //PROF_FUNC_H
