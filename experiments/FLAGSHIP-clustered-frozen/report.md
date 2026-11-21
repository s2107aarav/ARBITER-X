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

frozen observation (bots see a shadow touch excluding strategy orders; matching and trade feedback remain)

PnL is marked from strategy-local cash and inventory at the horizon. Filled lots count delivered fill legs. Markouts average seed means with observed probes.

| cell | configured allocation (ns) | MM PnL (ticks*qty) | MM filled lots | MM markout | Taker PnL | Taker filled lots |
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
- MM filled lots across the slice: 32317.0 (md share 1) vs 30760.0 (md share 0): sample means differ
- taker (momentum) mean PnL across the slice: -18.0 -> -27.1
- exploratory taker endpoint: mean paired PnL diff -9.1 (raw sign-flip p=0.8750)
- The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.

## Interpretation

Compare the Poisson and frozen-observation controls as sensitivity checks. Compare the per-lot point estimate with the planned economic threshold and its paired PnL result with the adjusted test. A non-rejection does not establish equivalence.

## Reproducibility

`arbiter flagship --reproduce results.json --report report.md` recomputes the outputs and compares their bytes. The recorded provenance is carried into the comparison. Verify it against the original clean build separately.
