#include "sampling.h"
#include "php_prof.h"
#include "helpers.h"
#include "profile.pb-c.h"
#include "gzencode.h"

#include "php_main.h"
#include "zend_smart_string.h"
#include "fort.h"

#include <signal.h>

#define SAMPLING_HITS_DEFAULT_CAPACITY 4096
#define PPROF_SAMPLE_TYPES_COUNT 2

ZEND_EXTERN_MODULE_GLOBALS(prof)

typedef struct {
    HashTable map;
    int64_t i;
} pprof_string_table_map;

static int64_t get_string_table_map_index(pprof_string_table_map *string_table_map, zend_string *str) {
    zval *index = zend_hash_lookup(&string_table_map->map, str);
    if (ZVAL_IS_NULL(index)) {
        ZVAL_LONG(index, string_table_map->i++);
    }
    return Z_LVAL_P(index);
}

static int64_t get_string_table_map_str_index(pprof_string_table_map *string_table_map, const char *str, size_t len) {
    zend_string *s = zend_string_init(str, len, 0);
    zval *index = zend_hash_lookup(&string_table_map->map, s);
    zend_string_release(s);
    if (ZVAL_IS_NULL(index)) {
        ZVAL_LONG(index, string_table_map->i++);
    }
    return Z_LVAL_P(index);
}

static void prof_sigprof_handler(int signo);
static zend_always_inline int prof_compare_sampling_units(Bucket *f, Bucket *s);
static int prof_sampling_apply_threshold(zval *zv);

zend_result prof_sampling_init() {
    return SUCCESS;
}

zend_result prof_sampling_setup() {
    PROF_G(sampling_enabled) = true;
    zend_hash_init(&PROF_G(sampling_units), SAMPLING_HITS_DEFAULT_CAPACITY, NULL, NULL, 0);

    zend_signal(SIGPROF, prof_sigprof_handler);

    struct itimerval timeout;
    timeout.it_value.tv_sec = timeout.it_interval.tv_sec = 0;
    timeout.it_value.tv_usec = timeout.it_interval.tv_usec = PROF_G(config).sampling_interval;

    if (setitimer(ITIMER_PROF, &timeout, NULL)) {
        return FAILURE;
    }

    return SUCCESS;
}

zend_result prof_sampling_teardown() {
    PROF_G(sampling_enabled) = false;

    struct itimerval no_timeout;
    no_timeout.it_value.tv_sec = no_timeout.it_value.tv_usec = no_timeout.it_interval.tv_sec = no_timeout.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_PROF, &no_timeout, NULL)) {
        return FAILURE;
    }

    prof_sampling_unit *sampling_unit;
    ZEND_HASH_FOREACH_PTR(&PROF_G(sampling_units), sampling_unit) {
        zend_string_release(sampling_unit->filename);
        efree(sampling_unit);
    } ZEND_HASH_FOREACH_END();
    zend_hash_destroy(&PROF_G(sampling_units));

    return SUCCESS;
}

void prof_sampling_print_result_console() {
    zend_string *function_name;
    prof_sampling_unit *sampling_unit;
    zend_ulong total_samples = 0;

    ZEND_HASH_FOREACH_PTR(&PROF_G(sampling_units), sampling_unit) {
        total_samples += sampling_unit->hits;
    } ZEND_HASH_FOREACH_END();

    php_printf("total samples: %ld\n", total_samples);
    if (total_samples == 0) {
        return;
    }

    ft_set_memory_funcs(local_malloc, local_free);
    ft_table_t *table = ft_create_table();
    ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
    ft_set_border_style(table, FT_SOLID_ROUND_STYLE);

    ft_write_ln(table, "function", "hits");

    zend_hash_sort(&PROF_G(sampling_units), prof_compare_sampling_units, 0);
    HashTable *hits = ht_slice(&PROF_G(sampling_units), PROF_G(config).sampling_limit);

    ZEND_HASH_FOREACH_STR_KEY_PTR(hits, function_name, sampling_unit) {
        if (sampling_unit->hits <= PROF_G(config).sampling_threshold) {
            continue;
        }

        ft_printf_ln(table, "%s|%ld (%d%%)", ZSTR_VAL(function_name), sampling_unit->hits, (int)((float)sampling_unit->hits / total_samples * 100));
    } ZEND_HASH_FOREACH_END();

    php_printf("%s\n", ft_to_string(table));

    ft_destroy_table(table);
    zend_hash_destroy(hits);
    efree(hits);
}

void prof_sampling_print_result_callgrind() {
    FILE *fp = fopen("callgrind.out", "w");
    if (!fp) {
        prof_add_error("open file");
        return;
    }

    smart_string cmd = {0};
    for (int i = 0; i < SG(request_info).argc; ++i) {
        smart_string_appends(&cmd, SG(request_info).argv[i]);
        if (i + 1 < SG(request_info).argc) {
            smart_string_appendc(&cmd, ' ');
        }
    }
    smart_string_0(&cmd);

    fprintf(fp, "# callgrind format\nversion: 1\ncmd: %s\npart: 1\n\nevents: Hits\n\n", cmd.c);

    // todo summary

    zend_string *function_name;
    prof_sampling_unit *sampling_unit;
    ZEND_HASH_FOREACH_STR_KEY_PTR(&PROF_G(sampling_units), function_name, sampling_unit) {
        if (sampling_unit->hits <= PROF_G(config).sampling_threshold) {
            continue;
        }

        // todo name compression
        fprintf(fp, "fl=%s\nfn=%s\n%d %lu\n\n", ZSTR_VAL(sampling_unit->filename), ZSTR_VAL(function_name), sampling_unit->lineno, sampling_unit->hits);
    } ZEND_HASH_FOREACH_END();

    // todo totals

    smart_string_free(&cmd);
    fclose(fp);
}

void prof_sampling_print_result_pprof() {
    zend_ulong end_time = get_wall_time();
    if (!end_time) {
        prof_add_warning("get wall time");
        return;
    }

    FILE *fp = fopen("cpu.pprof", "w");
    if (!fp) {
        prof_add_error("open file");
        return;
    }

    zend_hash_apply(&PROF_G(sampling_units), prof_sampling_apply_threshold);
    size_t sampling_units_count = zend_hash_num_elements(&PROF_G(sampling_units));

    Perftools__Profiles__Profile profile = PERFTOOLS__PROFILES__PROFILE__INIT;

    pprof_string_table_map string_table_map;
    // function name and filename for every sampling unit + sample types
    zend_hash_init(&string_table_map.map, sampling_units_count * 2 + 4, NULL, NULL, 0);
    string_table_map.i = 1; // first element is empty string

    Perftools__Profiles__ValueType samples_sample_type = PERFTOOLS__PROFILES__VALUE_TYPE__INIT;
    samples_sample_type.type = get_string_table_map_str_index(&string_table_map, "samples", sizeof("samples"));
    samples_sample_type.unit = get_string_table_map_str_index(&string_table_map, "count", sizeof("count"));
    Perftools__Profiles__ValueType cpu_sample_type = PERFTOOLS__PROFILES__VALUE_TYPE__INIT;
    cpu_sample_type.type = get_string_table_map_str_index(&string_table_map, "cpu", sizeof("cpu"));
    cpu_sample_type.unit = get_string_table_map_str_index(&string_table_map, "nanoseconds", sizeof("nanoseconds"));

    Perftools__Profiles__ValueType *sample_types[PPROF_SAMPLE_TYPES_COUNT] = {&samples_sample_type, &cpu_sample_type};

    Perftools__Profiles__ValueType period_type = PERFTOOLS__PROFILES__VALUE_TYPE__INIT;
    period_type.type = get_string_table_map_str_index(&string_table_map, "cpu", sizeof("cpu"));
    period_type.unit = get_string_table_map_str_index(&string_table_map, "nanoseconds", sizeof("nanoseconds"));

    Perftools__Profiles__Sample **samples = emalloc(sizeof(Perftools__Profiles__Sample *) * sampling_units_count);
    Perftools__Profiles__Location **locations = emalloc(sizeof(Perftools__Profiles__Location *) * sampling_units_count);
    Perftools__Profiles__Function **functions = emalloc(sizeof(Perftools__Profiles__Function *) * sampling_units_count);
    size_t samples_i = 0;
    size_t functions_i = 0;
    size_t locations_i = 0;

    zend_string *function_name;
    prof_sampling_unit *sampling_unit;
    ZEND_HASH_FOREACH_STR_KEY_PTR(&PROF_G(sampling_units), function_name, sampling_unit) {
        // create function
        functions[functions_i] = emalloc(sizeof(Perftools__Profiles__Function));
        perftools__profiles__function__init(functions[functions_i]);
        functions[functions_i]->id = functions_i + 1;
        functions[functions_i]->name = functions[functions_i]->system_name = get_string_table_map_index(&string_table_map, function_name);
        functions[functions_i]->filename = get_string_table_map_index(&string_table_map, sampling_unit->filename);
        functions[functions_i]->start_line = sampling_unit->lineno;
        functions_i++;

        // create location
        Perftools__Profiles__Line **lines = ecalloc(1, sizeof(Perftools__Profiles__Line *));
        lines[0] = emalloc(sizeof(Perftools__Profiles__Line));
        perftools__profiles__line__init(lines[0]);
        lines[0]->function_id = functions_i;
        lines[0]->line = sampling_unit->lineno;

        locations[locations_i] = emalloc(sizeof(Perftools__Profiles__Location));
        perftools__profiles__location__init(locations[locations_i]);
        locations[locations_i]->id = locations_i + 1;
        locations[locations_i]->mapping_id = 1;
        locations[locations_i]->n_line = 1;
        locations[locations_i]->line = lines;
        locations_i++;

        // create sample
        uint64_t *sample_locations = ecalloc(1, sizeof(uint64_t));
        sample_locations[0] = locations[locations_i - 1]->id;
        int64_t *sample_values = ecalloc(PPROF_SAMPLE_TYPES_COUNT, sizeof(int64_t));
        sample_values[0] = (int64_t)sampling_unit->hits;
        sample_values[1] = (int64_t)(sampling_unit->hits * PROF_G(config).sampling_interval) * 1000; // us to ns

        samples[samples_i] = emalloc(sizeof(Perftools__Profiles__Sample));
        perftools__profiles__sample__init(samples[samples_i]);
        samples[samples_i]->n_location_id = 1;
        samples[samples_i]->location_id = sample_locations;
        samples[samples_i]->n_value = PPROF_SAMPLE_TYPES_COUNT;
        samples[samples_i]->value = sample_values;
        samples_i++;
    } ZEND_HASH_FOREACH_END();

    Perftools__Profiles__Mapping mapping = PERFTOOLS__PROFILES__MAPPING__INIT;
    mapping.id = 1;
    mapping.has_functions = true;
    Perftools__Profiles__Mapping *mappings[1];
    mappings[0] = &mapping;

    char *string_table[string_table_map.i];
    string_table[0] = ""; // first element must be empty string
    zend_string *str;
    zval *index;
    ZEND_HASH_FOREACH_STR_KEY_VAL(&string_table_map.map, str, index) {
        string_table[Z_LVAL_P(index)] = ZSTR_VAL(str);
    } ZEND_HASH_FOREACH_END();

    profile.n_sample_type = PPROF_SAMPLE_TYPES_COUNT;
    profile.sample_type = sample_types;
    profile.n_sample = samples_i;
    profile.sample = samples;
    profile.n_mapping = 1;
    profile.mapping = mappings;
    profile.n_location = locations_i;
    profile.location = locations;
    profile.n_function = functions_i;
    profile.function = functions;
    profile.n_string_table = string_table_map.i;
    profile.string_table = string_table;
    profile.time_nanos = PROF_G(start_time);
    profile.duration_nanos = (int64_t)(end_time - PROF_G(start_time));
    profile.period_type = &period_type;
    profile.period = PROF_G(config).sampling_interval;

    // pack protobuf profile
    size_t len = perftools__profiles__profile__get_packed_size(&profile);
    void *buf = emalloc(len);
    if (buf) {
        if (perftools__profiles__profile__pack(&profile, buf) == len) {
            // compress protobuf profile
            zend_string *gzprofile = gzencode(buf, len);
            if (gzprofile) {
                if (fwrite(ZSTR_VAL(gzprofile), ZSTR_LEN(gzprofile), 1, fp) < 1) {
                    prof_add_error("write to file");
                }
                zend_string_release(gzprofile);
            } else {
                prof_add_error("compress profile");
            }
        } else {
            prof_add_error("pack profile");
        }
    } else {
        prof_add_warning("malloc profile buf");
    }

    // free all
    for (int i = 0; i < functions_i; i++) {
        efree(functions[i]);
    }
    for (int i = 0; i < locations_i; i++) {
        efree(locations[i]->line[0]);
        efree(locations[i]->line);
        efree(locations[i]);
    }
    for (int i = 0; i < samples_i; i++) {
        efree(samples[i]->location_id);
        efree(samples[i]->value);
        efree(samples[i]);
    }
    efree(functions);
    efree(locations);
    efree(samples);
    efree(buf);
    zend_hash_destroy(&string_table_map.map);
    fclose(fp);
}

static void prof_sigprof_handler(int signo) {
    if (!PROF_G(sampling_enabled)) {
        return;
    }

    zend_execute_data *execute_data = EG(current_execute_data);

    while (execute_data && (!execute_data->func || !ZEND_USER_CODE(execute_data->func->type))) {
        execute_data = execute_data->prev_execute_data;
    }

    if (!execute_data) {
        return;
    }

    zend_string *function_name = get_function_name(execute_data->func);
    if (!function_name) {
        prof_add_warning("get function name");
        return;
    }

    prof_sampling_unit *sampling_unit = zend_hash_find_ptr(&PROF_G(sampling_units), function_name);
    if (!sampling_unit) {
        sampling_unit = emalloc(sizeof(prof_sampling_unit));
        memset(sampling_unit, 0, sizeof(prof_sampling_unit));
        sampling_unit->filename = zend_string_copy(execute_data->func->op_array.filename);
        sampling_unit->lineno = execute_data->func->op_array.line_start;
        zend_hash_add_new_ptr(&PROF_G(sampling_units), function_name, sampling_unit);
    }

    sampling_unit->hits += 1;

    zend_string_release(function_name);
}

static zend_always_inline int prof_compare_sampling_units(Bucket *f, Bucket *s) {
    prof_sampling_unit *sampling_unit_f = (prof_sampling_unit *)Z_PTR(f->val);
    prof_sampling_unit *sampling_unit_s = (prof_sampling_unit *)Z_PTR(s->val);

    return sampling_unit_f->hits == sampling_unit_s->hits ? 0 : (sampling_unit_f->hits > sampling_unit_s->hits ? -1 : 1);
}

static int prof_sampling_apply_threshold(zval *zv) {
    prof_sampling_unit *sampling_unit = Z_PTR_P(zv);
    if (sampling_unit->hits <= PROF_G(config).sampling_threshold) {
        return ZEND_HASH_APPLY_REMOVE;
    }

    return ZEND_HASH_APPLY_KEEP;
}
