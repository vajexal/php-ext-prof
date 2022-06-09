#include "php_prof.h"
#include "errors.h"

ZEND_EXTERN_MODULE_GLOBALS(prof)

void prof_init_errors() {
    zend_hash_init(&PROF_G(errors), HT_MIN_SIZE, NULL, NULL, 0);
}

void prof_shutdown_errors() {
    prof_error *error;
    ZEND_HASH_FOREACH_PTR(&PROF_G(errors), error) {
        efree(error);
    } ZEND_HASH_FOREACH_END();

    zend_hash_destroy(&PROF_G(errors));
}

static void prof_add_error_message(prof_error_level level, const char *message) {
    prof_error *error = emalloc(sizeof(prof_error));
    error->level = level;
    error->message = message;
    zend_hash_next_index_insert_ptr(&PROF_G(errors), error);
}

void prof_add_warning(const char *message) {
    prof_add_error_message(PROF_ERROR_LEVEL_WARNING, message);
}

void prof_add_error(const char *message) {
    prof_add_error_message(PROF_ERROR_LEVEL_ERROR, message);
}

bool prof_has_errors() {
    return zend_array_count(&PROF_G(errors)) > 0;
}

void prof_print_errors() {
    if (!prof_has_errors()) {
        return;
    }

    bool has_errors = false;
    prof_error *error;
    ZEND_HASH_FOREACH_PTR(&PROF_G(errors), error) {
        if (error->level > PROF_ERROR_LEVEL_WARNING) {
            php_printf("Profile error: %s\n", error->message);
            has_errors = true;
        }
    } ZEND_HASH_FOREACH_END();

    if (!has_errors) {
        php_printf("There was error during profiling\n");
    }
}
