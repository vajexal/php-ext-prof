/* prof extension for PHP */

#ifndef PHP_PROF_H
# define PHP_PROF_H

#include "php.h"
#include "prof_config.h"

#include <pthread.h>
#include <stdatomic.h>

extern zend_module_entry prof_module_entry;
# define phpext_prof_ptr &prof_module_entry

# define PHP_PROF_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_PROF)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

typedef struct {
    zend_ulong wall;
    zend_ulong cpu;
    zend_ulong memory;
} prof_unit;

ZEND_BEGIN_MODULE_GLOBALS(prof)
    prof_config config;
    zend_array errors;
    zend_ulong start_time; // wall time in nanoseconds

    bool sampling_enabled;
    pthread_t sampling_thread;
    atomic_uint sampling_ticks;
    HashTable sampling_units;

    zend_stack func_start_units;
    HashTable func_units;

    zend_string *opcode_last_file;
    uint32_t opcode_last_lineno;
    zend_ulong opcode_last_time;
    HashTable opcode_timings;
ZEND_END_MODULE_GLOBALS(prof)

#ifdef ZTS
#define PROF_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(prof, v)
#else
#define PROF_G(v) (prof_globals.v)
#endif

#endif	/* PHP_PROF_H */
