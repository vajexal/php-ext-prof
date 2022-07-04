#include "php_prof.h"
#include "prof_config.h"

#include "SAPI.h"

ZEND_EXTERN_MODULE_GLOBALS(prof)

static bool parse_uint(zend_string *value, zend_ulong *res) {
    char *end;
    zend_long long_val = ZEND_STRTOL(ZSTR_VAL(value), &end, 0);
    if (end != ZSTR_VAL(value) + ZSTR_LEN(value)) {
        return false;
    }
    if (long_val < 0) {
        return false;
    }
    *res = long_val;
    return true;
}

static bool parse_double(zend_string *value, double *res) {
    char *end;
    double double_val = zend_strtod(ZSTR_VAL(value), (const char **)&end);
    if (end != ZSTR_VAL(value) + ZSTR_LEN(value)) {
        return false;
    }
    if (double_val < 0) {
        return false;
    }
    *res = double_val;
    return true;
}

static zend_string *get_config_var(const char *ini_name, const char *env_name) {
    zend_string *ini_name_str = zend_string_init(ini_name, strlen(ini_name), 0);
    zend_string *ini_value = zend_ini_get_value(ini_name_str);
    zend_string_release(ini_name_str);
    if (ini_value && ZSTR_LEN(ini_value) > 0) {
        return ini_value;
    }

    char *env_value = getenv(env_name);
    if (env_value && env_value[0]) {
        return zend_string_init(env_value, strlen(env_value), 0);
    }

    return NULL;
}

zend_result build_config() {
    PROF_G(config).mode = PROF_MODE_NONE;

    if (sapi_module.name && strcmp(sapi_module.name, "cli") != 0) {
        return SUCCESS;
    }

    zend_string *mode = get_config_var("prof.mode", "PROF_MODE");
    if (mode) {
        if (zend_string_equals_literal(mode, "sampling")) {
            PROF_G(config).mode = PROF_MODE_SAMPLING;
        } else if (zend_string_equals_literal(mode, "func")) {
            PROF_G(config).mode = PROF_MODE_FUNC;
        } else if (zend_string_equals_literal(mode, "opcode")) {
            PROF_G(config).mode = PROF_MODE_OPCODE;
        } else {
            zend_error(E_WARNING, "invalid prof mode");
            return FAILURE;
        }
    }

    PROF_G(config).output_mode = PROF_OUTPUT_MODE_CONSOLE;
    zend_string *output_mode = get_config_var("prof.output_mode", "PROF_OUTPUT_MODE");
    if (output_mode) {
        if (zend_string_equals_literal(output_mode, "console")) {
            PROF_G(config).output_mode = PROF_OUTPUT_MODE_CONSOLE;
        } else if (zend_string_equals_literal(output_mode, "callgrind")) {
            PROF_G(config).output_mode = PROF_OUTPUT_MODE_CALLGRIND;
        } else if (zend_string_equals_literal(output_mode, "pprof")) {
            PROF_G(config).output_mode = PROF_OUTPUT_MODE_PPROF;
        } else {
            zend_error(E_WARNING, "invalid prof output mode");
            return FAILURE;
        }
    }

    PROF_G(config).sampling_interval = 1000;
    zend_string *sampling_interval = get_config_var("prof.sampling_interval", "PROF_SAMPLING_INTERVAL");
    if (sampling_interval) {
        if (!parse_uint(sampling_interval, &PROF_G(config).sampling_interval) || PROF_G(config).sampling_interval < 100) {
            zend_error(E_WARNING, "invalid prof sampling interval");
            return FAILURE;
        }
    }

    PROF_G(config).sampling_limit = 20;
    zend_string *sampling_limit = get_config_var("prof.sampling_limit", "PROF_SAMPLING_LIMIT");
    if (sampling_limit) {
        if (!parse_uint(sampling_limit, &PROF_G(config).sampling_limit) || PROF_G(config).sampling_limit < 1) {
            zend_error(E_WARNING, "invalid prof sampling limit");
            return FAILURE;
        }
    }

    PROF_G(config).sampling_threshold = 1;
    zend_string *sampling_threshold = get_config_var("prof.sampling_threshold", "PROF_SAMPLING_THRESHOLD");
    if (sampling_threshold) {
        if (!parse_uint(sampling_threshold, &PROF_G(config).sampling_threshold)) {
            zend_error(E_WARNING, "invalid prof sampling threshold");
            return FAILURE;
        }
    }

    PROF_G(config).func_limit = 50;
    zend_string *func_limit = get_config_var("prof.func_limit", "PROF_FUNC_LIMIT");
    if (func_limit) {
        if (!parse_uint(func_limit, &PROF_G(config).func_limit) || PROF_G(config).func_limit < 1) {
            zend_error(E_WARNING, "invalid prof func limit");
            return FAILURE;
        }
    }

    PROF_G(config).func_threshold = 0.000001;
    zend_string *func_threshold = get_config_var("prof.func_threshold", "PROF_FUNC_THRESHOLD");
    if (func_threshold) {
        if (!parse_double(func_threshold, &PROF_G(config).func_threshold)) {
            zend_error(E_WARNING, "invalid prof func threshold");
            return FAILURE;
        }
    }

    PROF_G(config).opcode_threshold = 0;
    zend_string *opcode_threshold = get_config_var("prof.opcode_threshold", "PROF_OPCODE_THRESHOLD");
    if (opcode_threshold) {
        if (!parse_double(opcode_threshold, &PROF_G(config).opcode_threshold)) {
            zend_error(E_WARNING, "invalid prof opcode threshold");
            return FAILURE;
        }
    }

    return SUCCESS;
}
