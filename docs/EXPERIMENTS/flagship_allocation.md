# Experiment record: FLAGSHIP, latency-stage non-fungibility (fixed total)

- **Runner**: `arbiter flagship` (src/counterfactual/flagship.cpp).
- **Artifacts**: `experiments/FLAGSHIP-clustered-reactive/`,
  `experiments/FLAGSHIP-poisson-reactive/`,
  `experiments/FLAGSHIP-clustered-frozen/` are authoritative; regenerate,
  never hand-edit.
- **Reproduce**: `arbiter flagship --reproduce <dir>/results.json`
  (bit-for-bit when compiler and platform provenance match; all per-seed state
  hashes + economics).
- **Date**: 2026-11-12 (regenerated after correcting self-trade accounting;
  simulated outcomes are read from the authoritative artifacts).

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

* **Poisson null (k=0)**: every cell has identical mean economics (MM PnL 168.8, 171.9
  lots, all contrasts exactly 0, p=1): allocation has no effect under this
  null market. INTERPRETATION SCOPE: the regime contrast varies the taker
  process's dependence structure AND its realized intensity/sign structure
  together. The momentum archetype participates when its sign-signal threshold
  is reached, so the null result is
  NECESSARY but not SUFFICIENT evidence for the clustered-flow mechanism;
  it does not isolate dependence from intensity.
* **Clustered (k=0.9)**: cells move: MM PnL ranges -504663.8 to -223998.4
  ticks*qty, filled lots 26389.8–37557.5, markout -2.60 to -0.75 ticks. The
  primary contrast (extremes) has mean paired diff -122252.6 ticks*qty
  (-4.00 per filled lot; MATERIAL by the predefined threshold) but the
  Holm-corrected p-value is 0.1406: NOT statistically significant at 8
  seeds. 7/8 seed-level contrasts are negative; the differences remain
  heavy-tailed.
* **Interference sensitivity (frozen vs reactive)**: under frozen observation the
  clustered-regime point estimates shrink by an order of magnitude and the
  primary contrast is not material. The reactive-world point estimate is
  therefore observation-feedback-dependent, the central interpretation
  must carry this caveat.
* Adjacent contrasts alternate in sign, no monotone allocation surface at
  this budget/regime; the grid does not support claims of an "optimal
  allocation".

## Verdict

**UNCERTAIN / UNDERPOWERED.** H0 is NOT REJECTED at the primary contrast:
the point estimate is economically large, but cross-seed variance swamps it
(low power; report as uncertain; raw p=0.0156, Holm p=0.1406). The
Poisson null shows exactly zero placement sensitivity, so the instrument is
not dead, but the flagship data do not license a
non-fungibility claim at L=8µs in this market. How to read it:

1. The engine CAN express the allocation intervention exactly (verified).
2. The Poisson null market produces exactly zero placement sensitivity
   (necessary, not sufficient, for the clustered-flow mechanism, see the
   regime-contrast scope under Results).
3. Under clustered flow the point estimates move materially but are not
   statistically separable from zero at this replication.

## Kill conditions checked

- (B) mechanism contradiction: NOT triggered in the narrow sense, no
  effect under the Poisson null while the clustered regime moves. Caveat:
  because the regimes also differ in intensity, a null-market effect being
  absent cannot by itself confirm the dependence-structure mechanism.
- (C) economic threshold: the point estimate is material and the unadjusted
  IID interval excludes zero, while the Holm-corrected primary test remains
  inconclusive. It is reported as uncertain.
- (D) interference instability: PARTIAL, frozen-mode effects are ~10x
  smaller; the reactive result is feedback-dependent (documented above).
- (E) single-seed dependence: checked: 7/8 seed-level contrasts are negative;
  the per-seed differences remain heavy-tailed.

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
(this experiment's verdict is UNCERTAIN/UNDERPOWERED).

Eight seeds use 256 exact sign assignments. The configured 5000 repetitions
apply to larger-sample Monte Carlo tests. Identical economic means do not
imply identical per-cell state hashes or fill-event counts.
