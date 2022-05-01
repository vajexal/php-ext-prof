#ifndef PROF_HELPERS_H
#define PROF_HELPERS_H

#include "php.h"

#define MIN_KEY_COLUMN_LENGTH 20
#define MAX_KEY_COLUMN_LENGTH 120

zend_ulong get_time();
uint8_t get_prof_mode();
zend_string *get_function_name(zend_function *func);
int prof_compare_reverse_numeric_unstable_i(Bucket *f, Bucket *s);
uint16_t get_prof_key_column_length(HashTable *profile);
void prof_print_common_header();
HashTable *ht_slice(HashTable *ht, zend_ulong limit);

#endif //PROF_HELPERS_H
