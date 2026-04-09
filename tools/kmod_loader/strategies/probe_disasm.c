/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../resolver.h"

resolved_t strategy_probe_disasm(value_id_t id, resolve_ctx_t *ctx)
{
    (void)id; (void)ctx;
    return (resolved_t){ .available = 0 };
}
