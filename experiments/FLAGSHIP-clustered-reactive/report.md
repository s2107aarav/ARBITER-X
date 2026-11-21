# Flagship: latency-stage non-fungibility (fixed total budget)

This paired simulation study estimates effects within the model.

Source revision `8daa6da97f840b67178dfc5b2e089e2cac225482` (clean), built with clang 17.0.0 (clang-1700.6.3.2) on arm64-apple.

## Research question

Does shifting a fixed 8000 ns budget across observation, decision and order transit change mean market-maker marked PnL?

## Intervention

The allocation grid varies the observation share and divides the remainder between decision and order transit. Every configured allocation sums to the fixed budget. JSON reports measured publication-to-feed-arrival means for accepted MD packets excluding retransmissions. Decision-to-departure and departure-to-gateway means use delivered strategy fill legs with stage traces. Sample counts identify the two populations, and absent measurements are null. These separate means do not trace a single path.

## Market process

flow_model=clustered (post-trade intensity boost and trade-sign persistence, k=0.90)

The regime comparison jointly changes temporal dependence, realized intensity and trade-sign persistence. The Poisson control alone cannot identify the mechanism.

## Replication and inference

The 8 master seeds are the independent replication units. All cells use the same seeds and named streams for paired comparisons. The two-sided sign-flip test assumes the paired differences are invariant to independent sign changes under the null. Holm correction covers the 9 market-maker contrasts in this grid at family-wise alpha 0.05. The endpoints form the primary contrast. Intervals are descriptive, unadjusted normal approximations across seeds. The taker endpoint is exploratory.

The exact test uses 256 sign assignments. The smallest possible Holm-adjusted p-value is 0.0703125, before any additional ties. This pilot cannot reject at family-wise alpha 0.05.

## Interference mode

reactive (bots observe the live touch and react to strategy quotes; the measured effect includes market feedback — interference sensitivity is probed by re-running frozen)

PnL is marked from strategy-local cash and inventory at the horizon. Filled lots count delivered fill legs. Markouts average seed means with observed probes.

| cell | configured allocation (ns) | MM PnL (ticks*qty) | MM filled lots | MM markout | Taker PnL | Taker filled lots |
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
- MM filled lots across the slice: 38322.6 (md share 1) vs 37224.0 (md share 0): sample means differ
- taker (momentum) mean PnL across the slice: 77327.5 -> 132426.5
- exploratory taker endpoint: mean paired PnL diff +55099.0 (raw sign-flip p=0.5547)
- The primary contrast is inconclusive after Holm correction. The point estimate meets the materiality threshold.

## Interpretation

Compare the Poisson and frozen-observation controls as sensitivity checks. Compare the per-lot point estimate with the planned economic threshold and its paired PnL result with the adjusted test. A non-rejection does not establish equivalence.

## Reproducibility

`arbiter flagship --reproduce results.json --report report.md` recomputes the outputs and compares their bytes. The recorded provenance is carried into the comparison. Verify it against the original clean build separately.
