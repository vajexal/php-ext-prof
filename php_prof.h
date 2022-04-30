/* prof extension for PHP */

#ifndef PHP_PROF_H
# define PHP_PROF_H

#include <pthread.h>
#include <stdatomic.h>

extern zend_module_entry prof_module_entry;
# define phpext_prof_ptr &prof_module_entry

# define PHP_PROF_VERSION "0.1.0"

#define PROF_MODE_NONE 0
#define PROF_MODE_SAMPLING 1
#define PROF_MODE_FUNC 2
#define PROF_MODE_OPCODE 3

# if defined(ZTS) && defined(COMPILE_DL_PROF)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

ZEND_BEGIN_MODULE_GLOBALS(prof)
    uint8_t mode;
    bool error;
    zend_ulong start_time;

    bool sampling_enabled;
    uint sampling_interval;
    pthread_t sampling_thread;
    atomic_uint sampling_ticks;
    HashTable sampling_hits;

    zend_stack func_start_times;
    HashTable func_timings;

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
