#ifndef PROF_PROF_CONFIG_H
#define PROF_PROF_CONFIG_H

#include "php.h"

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

typedef struct {
    prof_mode mode;
    prof_output_mode output_mode;
    zend_ulong sampling_interval;
    zend_ulong sampling_limit;
    zend_ulong sampling_threshold;
    zend_ulong func_limit;
    double func_threshold;
} prof_config;

zend_result build_config();

#endif //PROF_PROF_CONFIG_H
