/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../resolver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Canned strategies for testing. */
static int call_log[8];
static int call_count = 0;

static resolved_t strategy_always_fail(value_id_t id, resolve_ctx_t *ctx) {
    (void)id; (void)ctx;
    call_log[call_count++] = 0;
    return (resolved_t){ .available = 0 };
}
static resolved_t strategy_always_42(value_id_t id, resolve_ctx_t *ctx) {
    (void)id; (void)ctx;
    call_log[call_count++] = 42;
    resolved_t r = { .available = 1, .u64_val = 42 };
    strcpy(r.source_label, "always_42");
    return r;
}

/* Stub out the real strategy symbols that resolver.c forward-declares.
 * The test provides its own g_value_specs[] (below), so these are only
 * needed to satisfy the linker — they are never invoked. */
resolved_t strategy_cli_override(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_probe_procfs(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_probe_loaded_module(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_probe_ondisk_module(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_probe_disasm(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_probe_binary_search(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_config_explicit(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_config_automatch(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }
resolved_t strategy_config_fuzzy(value_id_t id, resolve_ctx_t *ctx)
{ (void)id; (void)ctx; return (resolved_t){ .available = 0 }; }

/* Override g_value_specs with a test chain for VAL_MODULE_LAYOUT_CRC.
 * resolver.c omits its own g_value_specs[] when KH_RESOLVER_DEFINE_SPECS
 * is undefined (the test binary does NOT pass that flag). */
const value_spec_t g_value_specs[VAL__COUNT] = {
    [VAL_MODULE_LAYOUT_CRC] = { VAL_MODULE_LAYOUT_CRC, "test",
        { { strategy_always_fail, "fail" },
          { strategy_always_42,   "42"   },
          { 0, 0 } } },
};

int main(void)
{
    resolve_ctx_t ctx = {0};
    trace_entry_t trace;
    resolved_t r = resolve(VAL_MODULE_LAYOUT_CRC, &ctx, &trace);

    assert(r.available == 1);
    assert(r.u64_val == 42);
    assert(call_count == 2);
    assert(trace.tried_count == 2);
    assert(trace.ok == 1);
    assert(strcmp(trace.final.source_label, "always_42") == 0);

    printf("resolver_test: OK\n");
    return 0;
}
