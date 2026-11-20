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
| md0.000 | md+0,dec+4000,ord+4000 | -240235.1 | 34804.5 | -1.39 | 64125.0 | 2038.9 |
| md0.125 | md+1000,dec+3500,ord+3500 | -300133.6 | 35512.5 | -1.49 | 125148.4 | 2266.8 |
| md0.250 | md+2000,dec+3000,ord+3000 | -195298.0 | 33264.9 | -0.83 | 141667.4 | 1966.0 |
| md0.375 | md+3000,dec+2500,ord+2500 | -287842.1 | 35849.4 | -1.63 | 25202.1 | 2368.9 |
| md0.500 | md+4000,dec+2000,ord+2000 | -186236.6 | 31932.0 | -1.15 | 132794.9 | 2032.2 |
| md0.625 | md+5000,dec+1500,ord+1500 | -274529.0 | 40167.1 | -1.79 | -2772.1 | 2555.6 |
| md0.750 | md+6000,dec+1000,ord+1000 | -256601.8 | 40351.2 | -1.93 | 57431.2 | 2556.4 |
| md0.875 | md+7000,dec+500,ord+500 | -227099.4 | 35674.5 | -0.94 | 30978.5 | 1971.2 |
| md1.000 | md+8000,dec+0,ord+0 | -215654.2 | 32639.9 | -0.81 | 143619.1 | 1605.0 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | -59898.5 | [-226239.7, +106442.7] | 0.5781 | 1.0000 | -1.70 | no | yes |
| md0.125 → md0.250 | +104835.6 | [-39474.5, +249145.7] | 0.2500 | 1.0000 | +3.05 | no | yes |
| md0.250 → md0.375 | -92544.1 | [-144563.2, -40525.1] | 0.0156 | 0.1406 | -2.68 | no | yes |
| md0.375 → md0.500 | +101605.5 | [+52310.4, +150900.6] | 0.0234 | 0.1875 | +3.00 | no | yes |
| md0.500 → md0.625 | -88292.4 | [-171611.2, -4973.6] | 0.0703 | 0.4922 | -2.45 | no | yes |
| md0.625 → md0.750 | +17927.2 | [-60785.5, +96640.0] | 0.6562 | 1.0000 | +0.45 | no | no |
| md0.750 → md0.875 | +29502.4 | [-82065.4, +141070.1] | 0.6094 | 1.0000 | +0.78 | no | no |
| md0.875 → md1.000 | +11445.1 | [-94438.1, +117328.4] | 0.8438 | 1.0000 | +0.34 | no | no |
| md0.000 → md1.000 | +24580.9 | [-83517.8, +132679.6] | 0.6875 | 1.0000 | +0.73 | no | no |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff +24580.9 ticks*qty (per-lot +0.73), Holm p=1.0000, not significant
- economic materiality (primary): not material (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 32639.9 (md share 1) vs 34804.5 (md share 0): placement changes fill mechanics
- taker (momentum) PnL across the slice: 64125.0 -> 143619.1 (moves)
- taker primary-contrast paired PnL diff: +79494.1 (se 52335.3, perm p=0.1641) — not significant
- The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.

## Falsification status

- Poisson null: run the same grid with `flow_model=poisson`. Interpretation scope: if an allocation effect existed in the null market it would indicate placement sensitivity WITHOUT clustered flow (mechanism contradiction); a null result here is necessary but NOT sufficient for the clustered-flow mechanism (the regimes also differ in intensity — see the regime-contrast scope above).
- Interference: re-run with `bots_observe_shadow=true`; a large gap between reactive and frozen effects means the interpretation is unstable.
- Kill conditions: no adjacent-pair materiality under clustered flow at the primary contrast; or materiality driven by one seed; or reference/optimized book disagreement (checked separately in the differential suite).

## Reproducibility

`arbiter flagship --config <file>` re-runs this design; `arbiter flagship --reproduce <artifact.json>` verifies every recorded hash bit-for-bit. definition_hash covers the full design (grid, seeds, regime, horizon, thresholds).
