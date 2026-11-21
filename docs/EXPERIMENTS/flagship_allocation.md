# Experiment record: FLAGSHIP, latency-stage non-fungibility (fixed total)

- **Runner**: `arbiter flagship` (src/counterfactual/flagship.cpp).
- **Artifacts**: `experiments/FLAGSHIP-clustered-reactive/`,
  `experiments/FLAGSHIP-poisson-reactive/`,
  `experiments/FLAGSHIP-clustered-frozen/` are authoritative; regenerate,
  never hand-edit.
- **Reproduce**: `arbiter flagship --reproduce <dir>/results.json`
  (bit-for-bit when compiler and platform provenance match; all per-seed state
  hashes + economics).
- **Date**: 2026-11-21. The runner regenerated all three records after excluding filled provider orders from cancellation targets.

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

* **Poisson null (k=0).** The primary contrast was −0.02 ticks per filled
  lot with Holm p=1.0. The regime contrast varies both dependence and realized
  intensity, and the momentum strategy participates only when it sees a
  qualifying signal.
* **Clustered, reactive (k=0.9).** Mean MM PnL ranged from −259,680.9 to
  −189,187.1 ticks×qty. Mean filled lots ranged from 35,054.0 to 40,641.4
  and mean markouts from +0.20 to +0.33 ticks. Moving the full budget from
  action to observation gave a mean paired difference of −51,026.1
  ticks×qty, or −1.35 per filled lot. Six of eight differences were
  negative. The descriptive IID 95% interval was [−165,230.5, +63,178.2].
  The raw permutation p-value was 0.3984375 and the Holm-adjusted p-value was 1.0.
* **Clustered, frozen.** The same contrast gave +1,821.3 ticks×qty, or
  +0.06 per filled lot, with raw p=0.1015625 and Holm p=0.9140625.
  The change in sign and magnitude shows sensitivity to observation feedback.

## Verdict

The reactive point estimate exceeds the 1 tick per filled lot materiality
threshold in magnitude. The reactive and frozen contrasts are statistically
inconclusive after Holm correction across eight paired seeds.

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
