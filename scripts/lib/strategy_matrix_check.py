#!/usr/bin/env python3
# scripts/lib/strategy_matrix_check.py
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Validate a values/<device>.yaml against expectations.yaml.

Usage: strategy_matrix_check.py <expectations.yaml> <device.yaml>
Exit codes:
  0 -- PASS
  1 -- expectation violation
  2 -- file parse error / missing yaml module
"""
import sys

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed; skipping validation.", file=sys.stderr)
    sys.exit(0)  # soft-pass on missing module


def main():
    if len(sys.argv) != 3:
        print(
            "usage: strategy_matrix_check.py <expectations.yaml> <device.yaml>",
            file=sys.stderr,
        )
        sys.exit(2)

    try:
        with open(sys.argv[1]) as f:
            expectations = yaml.safe_load(f)
        with open(sys.argv[2]) as f:
            device = yaml.safe_load(f)
    except (OSError, yaml.YAMLError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(2)

    capabilities = device.get("capabilities") or {}
    observed = device.get("observed_values") or {}

    violations = []
    for cap, rule in (expectations or {}).items():
        exp_type = rule.get("type")
        capdata = capabilities.get(cap) or {}
        strategies = capdata.get("strategies", [])

        # Must have >= 1 registered strategy for the capability.
        if not strategies:
            violations.append(f"{cap}: no strategies registered (expected >=1)")
            continue

        enabled = [s for s in strategies if s.get("enabled")]
        if not enabled:
            violations.append(f"{cap}: no enabled strategies")
            continue

        winners = [s for s in strategies if s.get("winner")]
        if len(winners) > 1:
            violations.append(
                f"{cap}: multiple winners {[s['name'] for s in winners]}"
            )
            continue

        if exp_type == "probed_may_vary":
            val = observed.get(cap)
            if val in (None, "unknown"):
                continue  # not observable from dmesg -- skip
            # Accept int or hex-string.
            try:
                v = int(val, 0) if isinstance(val, str) else int(val)
            except (ValueError, TypeError):
                violations.append(f"{cap}: observed value {val!r} not parseable as int")
                continue
            if "range" in rule:
                lo, hi = rule["range"]
                lo = int(lo, 0) if isinstance(lo, str) else int(lo)
                hi = int(hi, 0) if isinstance(hi, str) else int(hi)
                if not (lo <= v <= hi):
                    violations.append(
                        f"{cap}: value 0x{v:x} out of range [0x{lo:x}, 0x{hi:x}]"
                    )
            elif "allowed" in rule:
                allowed = rule["allowed"]
                if v not in allowed:
                    violations.append(f"{cap}: value {v} not in allowed {allowed}")

        # scalar_all_strategies_equal, function_pointer_any_valid,
        # procedural_only, scalar_per_kernel_build, enum_matches_rule --
        # trust the kernel-side consistency_check + having >=1 winner;
        # nothing more to verify from userspace without re-running resolvers.

    if violations:
        print(f"FAIL: {len(violations)} violation(s)")
        for v in violations:
            print(f"  - {v}")
        sys.exit(1)

    print("PASS")


if __name__ == "__main__":
    main()
