/* prof extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_prof.h"
#include "helpers.h"
#include "sampling.h"
#include "func.h"
#include "opcode.h"

#include "ext/standard/info.h"

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
    ZEND_PARSE_PARAMETERS_START(0, 0) \
    ZEND_PARSE_PARAMETERS_END()
#endif

ZEND_DECLARE_MODULE_GLOBALS(prof)

PHP_INI_BEGIN()
PHP_INI_ENTRY("prof.mode", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.output_mode", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.sampling_interval", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.sampling_limit", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.sampling_threshold", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.func_limit", "", PHP_INI_SYSTEM, NULL)
PHP_INI_ENTRY("prof.func_threshold", "", PHP_INI_SYSTEM, NULL)
PHP_INI_END()

PHP_MINIT_FUNCTION(prof) {
#if defined(COMPILE_DL_MY_EXTENSION) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    REGISTER_INI_ENTRIES();

    if (build_config() == FAILURE) {
        return FAILURE;
    }

    switch (PROF_G(config).mode) {
        case PROF_MODE_SAMPLING:
            return prof_sampling_init();
        case PROF_MODE_FUNC:
            return prof_func_init();
        case PROF_MODE_OPCODE:
            return prof_opcode_init();
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(prof) {
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_RINIT_FUNCTION(prof) {
#if defined(ZTS) && defined(COMPILE_DL_PROF)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    prof_init_errors();
    PROF_G(start_time) = get_wall_time();
    if (!PROF_G(start_time)) {
        return FAILURE;
    }

    switch (PROF_G(config).mode) {
        case PROF_MODE_SAMPLING:
            return prof_sampling_setup();
        case PROF_MODE_FUNC:
            return prof_func_setup();
        case PROF_MODE_OPCODE:
            return prof_opcode_setup();
    }

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(prof) {
    if (prof_has_errors()) {
        prof_print_errors();
    } else {
        if (PROF_G(config).mode != PROF_MODE_NONE && PROF_G(config).output_mode == PROF_OUTPUT_MODE_CONSOLE) {
            prof_print_common_header();
        }

        switch (PROF_G(config).mode) {
            case PROF_MODE_SAMPLING:
                switch (PROF_G(config).output_mode) {
                    case PROF_OUTPUT_MODE_CONSOLE:
                        prof_sampling_print_result_console();
                        break;
                    case PROF_OUTPUT_MODE_CALLGRIND:
                        prof_sampling_print_result_callgrind();
                        break;
                    case PROF_OUTPUT_MODE_PPROF:
                        prof_sampling_print_result_pprof();
                        break;
                }
                break;
            case PROF_MODE_FUNC:
                prof_func_print_result();
                break;
            case PROF_MODE_OPCODE: {
                prof_opcode_print_result();
                break;
            }
        }

        prof_print_errors();
    }

    prof_shutdown_errors();

    switch (PROF_G(config).mode) {
        case PROF_MODE_SAMPLING:
            return prof_sampling_teardown();
        case PROF_MODE_FUNC:
            return prof_func_teardown();
        case PROF_MODE_OPCODE: {
            return prof_opcode_teardown();
        }
    }

    return SUCCESS;
}

PHP_MINFO_FUNCTION(prof) {
    php_info_print_table_start();
    php_info_print_table_header(2, "prof support", "enabled");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

static const zend_function_entry ext_functions[] = {
    ZEND_FE_END
};

zend_module_entry prof_module_entry = {
    STANDARD_MODULE_HEADER,
    "prof",
    ext_functions,
    PHP_MINIT(prof),
    PHP_MSHUTDOWN(prof),
    PHP_RINIT(prof),
    PHP_RSHUTDOWN(prof),
    PHP_MINFO(prof),
    PHP_PROF_VERSION,
    PHP_MODULE_GLOBALS(prof),
    NULL,
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PROF
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(prof)
#endif
