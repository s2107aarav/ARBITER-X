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
| md0.000 | md+0,dec+4000,ord+4000 | 22367.4 | 29341.2 | +0.91 | -23.5 | 12.0 |
| md0.125 | md+1000,dec+3500,ord+3500 | 23342.8 | 30667.8 | +0.93 | -18.9 | 10.4 |
| md0.250 | md+2000,dec+3000,ord+3000 | 24067.9 | 31364.9 | +0.92 | -27.6 | 23.6 |
| md0.375 | md+3000,dec+2500,ord+2500 | 23656.1 | 30784.5 | +0.91 | -28.8 | 29.4 |
| md0.500 | md+4000,dec+2000,ord+2000 | 25435.6 | 32890.9 | +0.91 | -40.1 | 31.9 |
| md0.625 | md+5000,dec+1500,ord+1500 | 26096.8 | 33812.9 | +0.92 | -40.1 | 22.2 |
| md0.750 | md+6000,dec+1000,ord+1000 | 26315.6 | 33600.8 | +0.93 | -23.2 | 21.1 |
| md0.875 | md+7000,dec+500,ord+500 | 26186.1 | 33440.5 | +0.95 | -51.2 | 30.8 |
| md1.000 | md+8000,dec+0,ord+0 | 27427.4 | 35089.0 | +0.95 | -44.1 | 29.9 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | +975.4 | [-53.5, +2004.3] | 0.1250 | 0.8750 | +0.03 | no | no |
| md0.125 → md0.250 | +725.1 | [-292.3, +1742.5] | 0.2031 | 1.0000 | +0.02 | no | no |
| md0.250 → md0.375 | -411.8 | [-3390.1, +2566.6] | 0.7734 | 1.0000 | -0.01 | no | no |
| md0.375 → md0.500 | +1779.5 | [-1168.2, +4727.2] | 0.2891 | 1.0000 | +0.06 | no | no |
| md0.500 → md0.625 | +661.1 | [-1441.5, +2763.7] | 0.5391 | 1.0000 | +0.02 | no | no |
| md0.625 → md0.750 | +218.9 | [-2178.4, +2616.1] | 0.9141 | 1.0000 | +0.01 | no | no |
| md0.750 → md0.875 | -129.5 | [-1792.0, +1533.0] | 0.8984 | 1.0000 | -0.00 | no | no |
| md0.875 → md1.000 | +1241.2 | [+59.7, +2422.8] | 0.0781 | 0.6250 | +0.04 | no | no |
| md0.000 → md1.000 | +5060.0 | [+3987.0, +6133.0] | 0.0078 | 0.0703 | +0.16 | no | no |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff +5060.0 ticks*qty (per-lot +0.16), Holm p=0.0703, not significant
- economic materiality (primary): not material (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 35089.0 (md share 1) vs 29341.2 (md share 0): placement changes fill mechanics
- taker (momentum) PnL across the slice: -23.5 -> -44.1 (moves)
- taker primary-contrast paired PnL diff: -20.6 (se 11.6, perm p=0.0938) — not significant
- The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.

## Falsification status

- Poisson null: run the same grid with `flow_model=poisson`. Interpretation scope: if an allocation effect existed in the null market it would indicate placement sensitivity WITHOUT clustered flow (mechanism contradiction); a null result here is necessary but NOT sufficient for the clustered-flow mechanism (the regimes also differ in intensity — see the regime-contrast scope above).
- Interference: re-run with `bots_observe_shadow=true`; a large gap between reactive and frozen effects means the interpretation is unstable.
- Kill conditions: no adjacent-pair materiality under clustered flow at the primary contrast; or materiality driven by one seed; or reference/optimized book disagreement (checked separately in the differential suite).

## Reproducibility

`arbiter flagship --config <file>` re-runs this design; `arbiter flagship --reproduce <artifact.json>` verifies every recorded hash bit-for-bit. definition_hash covers the full design (grid, seeds, regime, horizon, thresholds).
