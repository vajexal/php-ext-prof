#ifndef PROF_HELPERS_H
#define PROF_HELPERS_H

#include "php_prof.h"
#include "errors.h"

#define MIN_KEY_COLUMN_LENGTH 20
#define MAX_KEY_COLUMN_LENGTH 120

prof_unit *get_prof_unit();
// return microseconds
zend_ulong get_time();
// returns nanoseconds
zend_ulong get_wall_time();
prof_mode get_prof_mode();
prof_output_mode get_prof_output_mode();
zend_string *get_function_name(zend_function *func);
int prof_compare_reverse_numeric_unstable_i(Bucket *f, Bucket *s);
void prof_print_common_header();
HashTable *ht_slice(HashTable *ht, zend_ulong limit);
void get_memory_with_units(zend_long memory, char *buf, size_t buf_len);
void *local_malloc(size_t size);
void local_free(void *ptr);

#if PHP_VERSION_ID < 80100
zval* ZEND_FASTCALL zend_hash_lookup(HashTable *ht, zend_string *key);
#endif

#endif //PROF_HELPERS_H
