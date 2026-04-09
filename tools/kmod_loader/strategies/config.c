/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../resolver.h"

resolved_t strategy_config_explicit(value_id_t id, resolve_ctx_t *ctx)
{
    (void)id; (void)ctx;
    return (resolved_t){ .available = 0 };
}

resolved_t strategy_config_automatch(value_id_t id, resolve_ctx_t *ctx)
{
    (void)id; (void)ctx;
    return (resolved_t){ .available = 0 };
}

resolved_t strategy_config_fuzzy(value_id_t id, resolve_ctx_t *ctx)
{
    (void)id; (void)ctx;
    return (resolved_t){ .available = 0 };
}
