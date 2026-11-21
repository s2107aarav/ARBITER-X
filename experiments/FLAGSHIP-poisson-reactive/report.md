# Flagship: latency-stage non-fungibility (fixed total budget)

This paired simulation study estimates effects within the model.

Source revision `8daa6da97f840b67178dfc5b2e089e2cac225482` (clean), built with clang 17.0.0 (clang-1700.6.3.2) on arm64-apple.

## Research question

Does shifting a fixed 8000 ns budget across observation, decision and order transit change mean market-maker marked PnL?

## Intervention

The allocation grid varies the observation share and divides the remainder between decision and order transit. Every configured allocation sums to the fixed budget. JSON reports measured publication-to-feed-arrival means for accepted MD packets excluding retransmissions. Decision-to-departure and departure-to-gateway means use delivered strategy fill legs with stage traces. Sample counts identify the two populations, and absent measurements are null. These separate means do not trace a single path.

## Market process

flow_model=poisson (memoryless taker process within each regime, k=0)

The regime comparison jointly changes temporal dependence, realized intensity and trade-sign persistence. The Poisson control alone cannot identify the mechanism.

## Replication and inference

The 8 master seeds are the independent replication units. All cells use the same seeds and named streams for paired comparisons. The two-sided sign-flip test assumes the paired differences are invariant to independent sign changes under the null. Holm correction covers the 9 market-maker contrasts in this grid at family-wise alpha 0.05. The endpoints form the primary contrast. Intervals are descriptive, unadjusted normal approximations across seeds. The taker endpoint is exploratory.

The exact test uses 256 sign assignments. The smallest possible Holm-adjusted p-value is 0.0703125, before any additional ties. This pilot cannot reject at family-wise alpha 0.05.

## Interference mode

reactive (bots observe the live touch and react to strategy quotes; the measured effect includes market feedback — interference sensitivity is probed by re-running frozen)

PnL is marked from strategy-local cash and inventory at the horizon. Filled lots count delivered fill legs. Markouts average seed means with observed probes.

| cell | configured allocation (ns) | MM PnL (ticks*qty) | MM filled lots | MM markout | Taker PnL | Taker filled lots |
|---|---|---|---|---|---|---|
| md0.000 | md+0,dec+4000,ord+4000 | 184.4 | 206.2 | +0.92 | 0.0 | 0.0 |
| md0.125 | md+1000,dec+3500,ord+3500 | 181.2 | 203.1 | +0.91 | 0.0 | 0.0 |
| md0.250 | md+2000,dec+3000,ord+3000 | 181.2 | 203.1 | +0.91 | 0.0 | 0.0 |
| md0.375 | md+3000,dec+2500,ord+2500 | 181.2 | 203.1 | +0.91 | 0.0 | 0.0 |
| md0.500 | md+4000,dec+2000,ord+2000 | 181.2 | 200.0 | +0.92 | 0.0 | 0.0 |
| md0.625 | md+5000,dec+1500,ord+1500 | 181.2 | 200.0 | +0.92 | 0.0 | 0.0 |
| md0.750 | md+6000,dec+1000,ord+1000 | 181.2 | 200.0 | +0.92 | 0.0 | 0.0 |
| md0.875 | md+7000,dec+500,ord+500 | 181.2 | 203.1 | +0.91 | 0.0 | 0.0 |
| md1.000 | md+8000,dec+0,ord+0 | 181.2 | 203.1 | +0.91 | 0.0 | 0.0 |

## Paired contrasts (MM PnL, b − a, per seed)

| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | significant | material |
|---|---|---|---|---|---|---|---|
| md0.000 → md0.125 | -3.1 | [-9.2, +3.0] | 1.0000 | 1.0000 | -0.02 | no | no |
| md0.125 → md0.250 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.250 → md0.375 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.375 → md0.500 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.500 → md0.625 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.625 → md0.750 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.750 → md0.875 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.875 → md1.000 | +0.0 | [+0.0, +0.0] | 1.0000 | 1.0000 | +0.00 | no | no |
| md0.000 → md1.000 | -3.1 | [-9.2, +3.0] | 1.0000 | 1.0000 | -0.02 | no | no |

- primary contrast (md0.000 -> md1.000): mean paired MM PnL diff -3.1 ticks*qty (per-lot -0.02), Holm p=1.0000, not significant
- economic materiality (primary): not material (threshold 1.00 ticks*qty/lot)
- MM filled lots across the slice: 203.1 (md share 1) vs 206.2 (md share 0): sample means differ
- taker (momentum) mean PnL across the slice: 0.0 -> 0.0
- exploratory taker endpoint: mean paired PnL diff +0.0 (raw sign-flip p=1.0000)
- The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.

## Interpretation

Compare the Poisson and frozen-observation controls as sensitivity checks. Compare the per-lot point estimate with the planned economic threshold and its paired PnL result with the adjusted test. A non-rejection does not establish equivalence.

## Reproducibility

`arbiter flagship --reproduce results.json --report report.md` recomputes the outputs and compares their bytes. The recorded provenance is carried into the comparison. Verify it against the original clean build separately.
