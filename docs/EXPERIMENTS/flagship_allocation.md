# Experiment record: FLAGSHIP, latency-stage non-fungibility (fixed total)

- **Runner**: `arbiter flagship` (src/counterfactual/flagship.cpp).
- **Artifacts**: `experiments/FLAGSHIP-clustered-reactive/`,
  `experiments/FLAGSHIP-poisson-reactive/`,
  `experiments/FLAGSHIP-clustered-frozen/` are authoritative; regenerate,
  never hand-edit.
- **Reproduce**: `arbiter flagship --reproduce <dir>/results.json`
  (bit-for-bit when compiler and platform provenance match; all per-seed state
  hashes + economics).
- **Date**: 2026-11-20. The runner regenerated all three records after corrections to partial fills, FIFO queue estimates, and quote sizing.

## Question

For a FIXED total trader-side latency budget L, does the ALLOCATION of L
across (observation, decision, order-transit) change execution economics?
H0: only the total matters. H1: placement matters.

## Design

* Grid: ONE-DIMENSIONAL SLICE of the 3-stage simplex, the md-share axis
  {0, .125, ..., 1} at fixed L = 8,000 ns, with decision and order-transit
  split evenly (decision/order trade-offs in the interior are NOT explored).
  Typed allocation with exact-sum invariant; jitter zeroed on md/order
  channels so placement is exact.
* Regimes: clustered (k=0.9) vs Poisson (k=0, the null process). See Results for the regime-contrast scope.
* Interference: reactive (default) vs frozen (bots observe a shadow touch).
* Replication: 8 macro-seeds (2026..2033); seed = unit of replication;
  cells paired by seed (common random numbers).
* Endpoints: primary = MM marked PnL paired diff at the extremes contrast
  (md0.000 vs md1.000); secondary = filled lots, markouts, taker PnL.
* Statistics: sign-permutation p-values across seeds (exact enumeration),
  Holm correction across the 9-contrast family, descriptive IID seed-level CIs, economic
  materiality threshold 1.0 ticks*qty per filled lot (predefined).
* Engine-verified intervention: realized per-stage means recorded per cell
  from the stage trace.

## Results (clustered, reactive, the flagship run)

* **Poisson null (k=0)**: every cell has identical mean economics (MM PnL 168.8, 168.8
  lots, all contrasts exactly 0, p=1): allocation has no effect under this
  null market. INTERPRETATION SCOPE: the regime contrast varies the taker
  process's dependence structure AND its realized intensity/sign structure
  together. The momentum archetype participates when its sign-signal threshold
  is reached, so the null result is
  NECESSARY but not SUFFICIENT evidence for the clustered-flow mechanism;
  it does not isolate dependence from intensity.
* **Clustered, reactive (k=0.9).** Mean MM PnL across cells ranged from
  −300,133.6 to −186,236.6 ticks×qty. Mean filled lots ranged from 31,932.0
  to 40,351.3 and mean markouts from −1.93 to −0.81 ticks. Moving the full
  budget from action to observation produced a mean paired difference of
  +24,580.9 ticks×qty, or +0.73 per filled lot. Five of eight differences
  were positive. The descriptive IID 95% interval was [−83,517.8, +132,679.6].
  The raw permutation p-value was 0.6875 and the Holm-adjusted p-value was 1.0.
* **Clustered, frozen.** The same contrast gave +5,060.0 ticks×qty, or
  +0.16 per filled lot. All eight differences were positive. The raw
  permutation p-value was 0.0078125 and the Holm-adjusted p-value was
  0.0703125. Observation feedback changes the size of the point estimate.
* Adjacent contrasts vary in sign. The grid does not establish an optimal
  allocation.

## Verdict

Both clustered point estimates fell below the prespecified threshold of
1 tick×qty per filled lot. Neither primary contrast reached significance
after Holm correction at 0.05. The reactive interval includes effects in
both directions. These eight paired seeds leave the placement question open.

## Limitations

Single instrument, stylized clustered process (one-shot self-excitation,
not Hawkes), momentum archetype trades only in the clustered regime,
L=8µs total (a sub-quote-lifetime regime), markout horizon 100µs. Cross-architecture
reproduction untested (same-arch/build claim only).

## Literature positioning

See docs/LITERATURE_POSITIONING.md (verified against arXiv records
2026-09-09): prior latency work treats latency as a scalar total or a
venue-imposed delay (Gao & Wang 2018; Lehalle & Mounjid 2016; Khapko &
Zoican 2019; Kelly & Yudovina 2015); the fixed-total allocation
decomposition was not found there. The contribution claimed is the
question + the deterministic instrument, NOT a positive empirical finding
(the primary contrast is inconclusive).

Eight seeds use 256 exact sign assignments. The configured 5000 repetitions
apply to larger-sample Monte Carlo tests. Identical economic means do not
imply identical per-cell state hashes or fill-event counts.
