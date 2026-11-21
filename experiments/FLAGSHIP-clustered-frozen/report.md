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

frozen observation (bots see a shadow touch excluding strategy orders; matching and trade feedback remain)

| cell | allocation | MM PnL (ticks*qty) | MM filled lots | MM markout | Taker PnL | Taker filled lots |
|---|---|---|---|---|---|---|
| md0.000 | md+0,dec+4000,ord+4000 | 26084.8 | 30760.0 | +0.93 | -18.0 | 14.0 |
| md0.125 | md+1000,dec+3500,ord+3500 | 25844.1 | 29865.1 | +0.93 | -7.0 | 5.4 |
| md0.250 | md+2000,dec+3000,ord+3000 | 26180.6 | 30472.0 | +0.94 | -4.4 | 7.8 |
| md0.375 | md+3000,dec+2500,ord+2500 | 26570.6 | 31138.4 | +0.94 | -15.4 | 23.5 |
| md0.500 | md+4000,dec+2000,ord+2000 | 27662.4 | 32465.9 | +0.94 | -24.5 | 16.0 |
| md0.625 | md+5000,dec+1500,ord+1500 | 27974.1 | 32574.6 | +0.95 | -18.2 | 18.9 |
| md0.750 | md+6000,dec+1000,ord+1000 | 26841.9 | 31281.6 | +0.94 | -48.5 | 38.0 |
| md0.875 | md+7000,dec+500,ord+500 | 27212.2 | 31674.4 | +0.94 | -42.1 | 26.1 |
| md1.000 | md+8000,dec+0,ord+0 | 27906.0 | 32317.0 | +0.94 | -27.1 | 13.6 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | -240.6 | [-1843.7, +1362.5] | 0.7656 | 1.0000 | -0.01 | no | no |
| md0.125 → md0.250 | +336.5 | [-445.8, +1118.8] | 0.4375 | 1.0000 | +0.01 | no | no |
| md0.250 → md0.375 | +390.0 | [-717.9, +1497.9] | 0.5234 | 1.0000 | +0.01 | no | no |
| md0.375 → md0.500 | +1091.8 | [-204.5, +2388.0] | 0.1562 | 1.0000 | +0.03 | no | no |
| md0.500 → md0.625 | +311.8 | [-1396.3, +2019.8] | 0.7422 | 1.0000 | +0.01 | no | no |
| md0.625 → md0.750 | -1132.2 | [-3117.2, +852.7] | 0.2891 | 1.0000 | -0.04 | no | no |
| md0.750 → md0.875 | +370.4 | [-1003.6, +1744.3] | 0.6250 | 1.0000 | +0.01 | no | no |
| md0.875 → md1.000 | +693.8 | [-474.9, +1862.4] | 0.2578 | 1.0000 | +0.02 | no | no |
| md0.000 → md1.000 | +1821.2 | [-192.1, +3834.6] | 0.1016 | 0.9141 | +0.06 | no | no |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff +1821.2 ticks*qty (per-lot +0.06), Holm p=0.9141, not significant
- economic materiality (primary): not material (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 32317.0 (md share 1) vs 30760.0 (md share 0): placement changes fill mechanics
- taker (momentum) PnL across the slice: -18.0 -> -27.1 (moves)
- taker primary-contrast paired PnL diff: -9.1 (se 28.3, perm p=0.8750) — not significant
- The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.

## Falsification status

- Poisson null: run the same grid with `flow_model=poisson`. Interpretation scope: if an allocation effect existed in the null market it would indicate placement sensitivity WITHOUT clustered flow (mechanism contradiction); a null result here is necessary but NOT sufficient for the clustered-flow mechanism (the regimes also differ in intensity — see the regime-contrast scope above).
- Interference: re-run with `bots_observe_shadow=true`; a large gap between reactive and frozen effects means the interpretation is unstable.
- Kill conditions: no adjacent-pair materiality under clustered flow at the primary contrast; or materiality driven by one seed; or reference/optimized book disagreement (checked separately in the differential suite).

## Reproducibility

`arbiter flagship --config <file>` re-runs this design; `arbiter flagship --reproduce <artifact.json>` verifies every recorded hash bit-for-bit. definition_hash covers the full design (grid, seeds, regime, horizon, thresholds).
