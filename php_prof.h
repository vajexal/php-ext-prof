/* prof extension for PHP */

#ifndef PHP_PROF_H
# define PHP_PROF_H

#include <pthread.h>
#include <stdatomic.h>

extern zend_module_entry prof_module_entry;
# define phpext_prof_ptr &prof_module_entry

# define PHP_PROF_VERSION "0.1.0"

typedef enum {
    PROF_MODE_NONE,
    PROF_MODE_SAMPLING,
    PROF_MODE_FUNC,
    PROF_MODE_OPCODE
} prof_mode;

typedef enum {
    PROF_OUTPUT_MODE_CONSOLE,
    PROF_OUTPUT_MODE_CALLGRIND,
    PROF_OUTPUT_MODE_PPROF
} prof_output_mode;

# if defined(ZTS) && defined(COMPILE_DL_PROF)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

typedef struct {
    zend_ulong wall;
    zend_ulong cpu;
    zend_ulong memory;
} prof_unit;

ZEND_BEGIN_MODULE_GLOBALS(prof)
    prof_mode mode;
    prof_output_mode output_mode;
    bool error;
    zend_ulong start_time; // wall time in nanoseconds

    bool sampling_enabled;
    uint sampling_interval;
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
