#ifndef PROF_ERRORS_H
#define PROF_ERRORS_H

#include <stdbool.h>

typedef enum {
    PROF_ERROR_LEVEL_WARNING,
    PROF_ERROR_LEVEL_ERROR
} prof_error_level;

typedef struct {
    prof_error_level level;
    const char *message;
} prof_error;

void prof_init_errors();
void prof_shutdown_errors();
void prof_add_warning(const char *message);
void prof_add_error(const char *message);
bool prof_has_errors();
void prof_print_errors();

#endif //PROF_ERRORS_H
