# Flagship: latency-stage non-fungibility (fixed total budget)

**This is a paired counterfactual estimate under the simulator's structural assumptions — not a causal claim about real markets.**

## Research question

For a FIXED total trader-side latency budget L = 8000 ns, does the ALLOCATION of L across (observation md_transit, decision, order_transit) change execution economics?

H0: only the total matters (E[Y|allocation] = f(L)). H1: placement matters (stage marginal effects differ).

## Intervention

ONE-DIMENSIONAL SLICE of the 3-stage allocation simplex: the md-share axis {0..1} with the action stages (decision, order_transit) split evenly — NOT the full simplex (interior decision/order trade-offs are not explored). Identical total in every cell, enforced by the typed `LatencyAllocation` invariant; engine-measured realized stage latencies recorded per cell.

## Market process

flow_model=clustered (post-trade intensity boost + trade-sign persistence, k=0.90; a minimal self-exciting mechanism with a bit-exact Poisson null — NOT a Hawkes process)

**Regime-contrast scope (limitation):** the two flow regimes differ in the taker process's dependence structure AND consequently in realized intensity and trade-sign runs. The clustered process can produce sign signals that activate the momentum strategy. The placebo therefore shows "allocation has no effect under THIS null market"; it does NOT isolate which aspect of the clustered process (dependence vs intensity) would drive any clustered-regime effect.

## Replication

Seed is the unit of replication (independent worlds; within-world events are never treated as independent). Cells are PAIRED by seed (common random numbers): same master seed, same named streams.

## Interference mode

reactive (bots observe the live touch and react to strategy quotes; the measured effect includes market feedback — interference sensitivity is probed by re-running frozen)

| cell | allocation | MM PnL (ticks*qty) | MM filled lots | MM markout | Taker PnL | Taker filled lots |
|---|---|---|---|---|---|---|
| md0.000 | md+0,dec+4000,ord+4000 | -208654.8 | 37224.0 | +0.33 | 77327.5 | 1835.2 |
| md0.125 | md+1000,dec+3500,ord+3500 | -189187.1 | 39082.5 | +0.29 | 75542.0 | 1682.8 |
| md0.250 | md+2000,dec+3000,ord+3000 | -240828.8 | 40641.4 | +0.28 | 8492.0 | 1981.0 |
| md0.375 | md+3000,dec+2500,ord+2500 | -194930.8 | 37009.1 | +0.32 | 67453.6 | 1626.5 |
| md0.500 | md+4000,dec+2000,ord+2000 | -247499.4 | 40303.2 | +0.21 | 104629.2 | 1933.0 |
| md0.625 | md+5000,dec+1500,ord+1500 | -237626.9 | 36573.0 | +0.20 | 102213.8 | 1989.9 |
| md0.750 | md+6000,dec+1000,ord+1000 | -225005.6 | 38859.8 | +0.22 | 91440.0 | 1906.0 |
| md0.875 | md+7000,dec+500,ord+500 | -190609.6 | 35054.0 | +0.24 | 129016.2 | 1681.1 |
| md1.000 | md+8000,dec+0,ord+0 | -259680.9 | 38322.6 | +0.26 | 132426.5 | 2030.6 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | +19467.6 | [-32263.7, +71198.9] | 0.4375 | 1.0000 | +0.51 | no | no |
| md0.125 → md0.250 | -51641.6 | [-116423.5, +13140.3] | 0.1406 | 1.0000 | -1.30 | no | yes |
| md0.250 → md0.375 | +45898.0 | [-15618.5, +107414.5] | 0.1953 | 1.0000 | +1.18 | no | yes |
| md0.375 → md0.500 | -52568.6 | [-119034.3, +13897.1] | 0.1719 | 1.0000 | -1.36 | no | yes |
| md0.500 → md0.625 | +9872.5 | [-65389.4, +85134.4] | 0.8438 | 1.0000 | +0.26 | no | no |
| md0.625 → md0.750 | +12621.2 | [-106828.7, +132071.2] | 0.8203 | 1.0000 | +0.33 | no | no |
| md0.750 → md0.875 | +34396.0 | [-33368.7, +102160.7] | 0.4141 | 1.0000 | +0.93 | no | no |
| md0.875 → md1.000 | -69071.2 | [-127392.1, -10750.4] | 0.0781 | 0.7031 | -1.88 | no | yes |
| md0.000 → md1.000 | -51026.1 | [-165230.5, +63178.2] | 0.3984 | 1.0000 | -1.35 | no | yes |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff -51026.1 ticks*qty (per-lot -1.35), Holm p=1.0000, not significant
- economic materiality (primary): MATERIAL (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 38322.6 (md share 1) vs 37224.0 (md share 0): placement changes fill mechanics
- taker (momentum) PnL across the slice: 77327.5 -> 132426.5 (moves)
- taker primary-contrast paired PnL diff: +55099.0 (se 91830.4, perm p=0.5547) — not significant
- The primary contrast is inconclusive after Holm correction. The point estimate meets the materiality threshold.

## Falsification status

- Poisson null: run the same grid with `flow_model=poisson`. Interpretation scope: if an allocation effect existed in the null market it would indicate placement sensitivity WITHOUT clustered flow (mechanism contradiction); a null result here is necessary but NOT sufficient for the clustered-flow mechanism (the regimes also differ in intensity — see the regime-contrast scope above).
- Interference: re-run with `bots_observe_shadow=true`; a large gap between reactive and frozen effects means the interpretation is unstable.
- Kill conditions: no adjacent-pair materiality under clustered flow at the primary contrast; or materiality driven by one seed; or reference/optimized book disagreement (checked separately in the differential suite).

## Reproducibility

`arbiter flagship --config <file>` re-runs this design; `arbiter flagship --reproduce <artifact.json>` verifies every recorded hash bit-for-bit. definition_hash covers the full design (grid, seeds, regime, horizon, thresholds).
