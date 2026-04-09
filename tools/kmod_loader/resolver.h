/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _KH_RESOLVER_H_
#define _KH_RESOLVER_H_

#include <stddef.h>
#include <stdint.h>
#include <elf.h>

#define KH_TRACE_MAX 32
#define KH_SOURCE_LABEL_MAX 128
#define KH_VERMAGIC_MAX 256

typedef enum {
    VAL_MODULE_LAYOUT_CRC = 0,
    VAL_PRINTK_CRC,
    VAL_MEMCPY_CRC,
    VAL_MEMSET_CRC,
    VAL_VERMAGIC,
    VAL_THIS_MODULE_SIZE,
    VAL_MODULE_INIT_OFFSET,
    VAL_MODULE_EXIT_OFFSET,
    VAL_KALLSYMS_LOOKUP_NAME_ADDR,
    VAL__COUNT
} value_id_t;

/* One value's resolved state. Numeric values live in u64_val; vermagic
 * lives in str_val. The producer strategy's label (for trace output)
 * lives in source_label. */
typedef struct {
    int         available;
    uint64_t    u64_val;
    char        str_val[KH_VERMAGIC_MAX];
    char        source_label[KH_SOURCE_LABEL_MAX];
} resolved_t;

/* Trace record: which strategies were tried, which one won, and the
 * final value's provenance label. Used by `info --verbose`. */
typedef struct {
    value_id_t  id;
    const char *display_name;
    char        tried[KH_TRACE_MAX][64];
    int         tried_count;
    resolved_t  final;
    int         ok;
} trace_entry_t;

/* Context passed to every strategy. Holds CLI flags, kernel identity,
 * and a pointer to the .ko buffer if the strategy needs to probe it. */
typedef struct {
    /* Kernel identity */
    int   kmajor, kminor;
    char  uname_release[128];

    /* CLI-resolved values (filled in by the argv parser) */
    int      have_module_layout_crc;
    uint32_t cli_module_layout_crc;
    int      have_printk_crc;
    uint32_t cli_printk_crc;
    int      have_memcpy_crc;
    uint32_t cli_memcpy_crc;
    int      have_memset_crc;
    uint32_t cli_memset_crc;
    int      have_vermagic;
    char     cli_vermagic[KH_VERMAGIC_MAX];
    int      have_this_module_size;
    uint32_t cli_this_module_size;
    int      have_module_init_offset;
    uint32_t cli_module_init_offset;
    int      have_module_exit_offset;
    uint32_t cli_module_exit_offset;
    int      have_kallsyms_addr;
    uint64_t cli_kallsyms_addr;

    /* Explicit --device=<name> if set */
    const char *device_override;

    /* Priority flip */
    int prefer_config;
    int no_probe;
    int no_config;
    int strict_config;

    /* Module binary under load (may be NULL if strategy doesn't need it) */
    uint8_t   *mod_buf;
    size_t     mod_size;
    Elf64_Ehdr *mod_eh;

    /* Cached device entry (if config_automatch / config_explicit ran) */
    const struct device_entry *selected_device;
} resolve_ctx_t;

/* Strategy function signature. Returns a resolved_t with available=1 if
 * it found the value, or available=0 if it couldn't. The strategy is
 * free to set source_label to any descriptive string. */
typedef resolved_t (*strategy_fn)(value_id_t id, resolve_ctx_t *ctx);

typedef struct {
    strategy_fn  fn;
    const char  *name;
} strategy_t;

typedef struct {
    value_id_t   id;
    const char  *display_name;
    strategy_t   chain[10];  /* NULL-fn terminated */
} value_spec_t;

extern const value_spec_t g_value_specs[VAL__COUNT];

/* Core API. */
resolved_t resolve(value_id_t id, resolve_ctx_t *ctx, trace_entry_t *trace_out);
const char *value_name(value_id_t id);
void trace_dump(const trace_entry_t *trace, int count);

#endif /* _KH_RESOLVER_H_ */
