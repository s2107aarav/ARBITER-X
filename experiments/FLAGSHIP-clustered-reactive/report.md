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
| md0.000 | md+0,dec+4000,ord+4000 | -223998.4 | 34756.4 | -1.15 | 108881.1 | 1838.2 |
| md0.125 | md+1000,dec+3500,ord+3500 | -304774.1 | 35923.2 | -1.32 | 52477.6 | 1972.8 |
| md0.250 | md+2000,dec+3000,ord+3000 | -504663.8 | 35491.5 | -2.52 | 52192.4 | 2363.6 |
| md0.375 | md+3000,dec+2500,ord+2500 | -341063.4 | 37557.5 | -1.63 | 1173.8 | 2388.9 |
| md0.500 | md+4000,dec+2000,ord+2000 | -362678.0 | 29834.5 | -0.75 | 101567.6 | 1601.2 |
| md0.625 | md+5000,dec+1500,ord+1500 | -347693.1 | 32311.2 | -0.93 | 50599.8 | 1789.0 |
| md0.750 | md+6000,dec+1000,ord+1000 | -356130.9 | 29680.1 | -1.12 | 100350.2 | 1797.6 |
| md0.875 | md+7000,dec+500,ord+500 | -369365.5 | 30230.0 | -2.60 | 51732.8 | 2091.0 |
| md1.000 | md+8000,dec+0,ord+0 | -346251.0 | 26389.8 | -1.08 | 41299.9 | 1645.0 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | -80775.8 | [-147060.3, -14491.2] | 0.0391 | 0.3125 | -2.29 | no | yes |
| md0.125 → md0.250 | -199889.6 | [-338729.1, -61050.2] | 0.0469 | 0.3281 | -5.60 | no | yes |
| md0.250 → md0.375 | +163600.4 | [+19672.9, +307527.9] | 0.0781 | 0.4688 | +4.48 | no | yes |
| md0.375 → md0.500 | -21614.6 | [-79221.2, +35992.0] | 0.5234 | 1.0000 | -0.64 | no | no |
| md0.500 → md0.625 | +14984.9 | [-92886.7, +122856.5] | 0.8438 | 1.0000 | +0.48 | no | no |
| md0.625 → md0.750 | -8437.8 | [-124822.8, +107947.3] | 0.8984 | 1.0000 | -0.27 | no | no |
| md0.750 → md0.875 | -13234.6 | [-123757.9, +97288.6] | 0.8047 | 1.0000 | -0.44 | no | no |
| md0.875 → md1.000 | +23114.5 | [-65267.2, +111496.2] | 0.6016 | 1.0000 | +0.82 | no | no |
| md0.000 → md1.000 | -122252.6 | [-212367.3, -32137.9] | 0.0156 | 0.1406 | -4.00 | no | yes |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff -122252.6 ticks*qty (per-lot -4.00), Holm p=0.1406, not significant
- economic materiality (primary): MATERIAL (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 26389.8 (md share 1) vs 34756.4 (md share 0): placement changes fill mechanics
- taker (momentum) PnL across the slice: 108881.1 -> 41299.9 (moves)
- taker primary-contrast paired PnL diff: -67581.2 (se 90422.2, perm p=0.5000) — not significant
- H0 at the primary contrast: NOT REJECTED — point estimate is economically large but cross-seed variance swamps it (low power; report as uncertain, do NOT interpret the sign as an effect)

## Falsification status

- Poisson null: run the same grid with `flow_model=poisson`. Interpretation scope: if an allocation effect existed in the null market it would indicate placement sensitivity WITHOUT clustered flow (mechanism contradiction); a null result here is necessary but NOT sufficient for the clustered-flow mechanism (the regimes also differ in intensity — see the regime-contrast scope above).
- Interference: re-run with `bots_observe_shadow=true`; a large gap between reactive and frozen effects means the interpretation is unstable.
- Kill conditions: no adjacent-pair materiality under clustered flow at the primary contrast; or materiality driven by one seed; or reference/optimized book disagreement (checked separately in the differential suite).

## Reproducibility

`arbiter flagship --config <file>` re-runs this design; `arbiter flagship --reproduce <artifact.json>` verifies every recorded hash bit-for-bit. definition_hash covers the full design (grid, seeds, regime, horizon, thresholds).
