# Experiment record: FLAGSHIP, latency-stage non-fungibility (fixed total)

- **Runner**: `arbiter flagship` (src/counterfactual/flagship.cpp).
- **Artifacts**: `experiments/FLAGSHIP-clustered-reactive/`,
  `experiments/FLAGSHIP-poisson-reactive/`,
  `experiments/FLAGSHIP-clustered-frozen/` are authoritative; regenerate,
  never hand-edit.
- **Reproduce** with `arbiter flagship --reproduce <dir>/results.json --report <dir>/report.md`.
  See [clean build and generation instructions](../REPRODUCIBILITY.md).
- **Status**. The retained eight-seed pilot is the basis for the follow-up
  design below. The proposed independent seeds have no recorded outcomes.

## Question

For a FIXED total trader-side latency budget L, does the ALLOCATION of L
across (observation, decision, order-transit) change execution economics?
H0: only the total matters. H1: placement matters.

## Design

* Grid: ONE-DIMENSIONAL SLICE of the 3-stage simplex, the md-share axis
  {0, .125, ..., 1} at fixed L = 8,000 ns, with decision and order-transit
  split evenly (decision/order trade-offs in the interior are NOT explored).
  The configured delays sum to the exact integer budget. MD and order jitter
  are zero in these runs.
* Regimes: clustered (k=0.9) vs Poisson (k=0, the null process). See Results for the regime-contrast scope.
* Interference: reactive (default) vs frozen (bots observe a shadow touch).
* Replication: 8 macro-seeds (2026..2033); seed = unit of replication;
  cells paired by seed (common random numbers).
* Endpoints: primary = MM marked PnL paired diff at the extremes contrast
  (md0.000 vs md1.000); secondary = filled lots, markouts, taker PnL.
* Statistics use a two-sided sign-flip test with the absolute mean paired
  PnL difference. Its null assumes the joint difference distribution is
  invariant to independent sign flips across seeds. Holm controls family-wise
  error at 0.05 across the eight adjacent contrasts and the endpoint contrast
  within the grid. Intervals are unadjusted normal approximations across
  seeds. The per-lot threshold is an economic reference for the point estimate.
* Intervention measurements use publication-to-feed-arrival times for accepted
  non-retransmit MD packets, and decision-to-departure and departure-to-gateway
  times for delivered strategy fill legs with traces. JSON reports the mean
  and sample count for each population. These populations cannot be added
  to obtain a measured three-stage path.

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
threshold in magnitude and its interval spans both signs. Exact enumeration
with eight pairs yields 256 assignments. An absolute statistic always
includes the observed sign assignment and its negative, so the smallest raw
p-value is 2/256 and the smallest Holm-adjusted p-value across nine contrasts
is 9 × 2/256 = 0.0703125. Ties can raise that floor. Thus this pilot cannot
reject at 0.05 and leaves latency-stage non-fungibility unresolved. Nine
informative pairs make rejection possible in principle.

## Proposed independent follow-up

Use **400 new master seeds** and keep the 8 µs grid, PnL endpoint, primary
contrast, two-sided sign flips and all nine Holm comparisons. Use each new
seed once per cell with common random numbers. Treat the Poisson and
frozen-observation results as sensitivity analyses. A confirmatory claim
about an interaction would need its own prespecified test family.

The pilot reactive endpoint differences had a sample SD of 164,805 ticks×lots.
The endpoint cells averaged 37,773 delivered MM lots per seed, so a 1 tick/lot
effect corresponds to a PnL shift of about 37,773 ticks×lots, or a standardized
shift of 0.229. For planning, use a two-sided normal approximation at the
first Holm threshold (0.05/9). With paired SD σ, target PnL shift Δ and
target power 1−β, take the ceiling of
`[(z(1 − 0.05/18) + z(1 − β)) × σ / Δ]²`.
These are sensitivity estimates using pilot dispersion and approximately
symmetric shifts. The PnL test does not test a ratio or economic equivalence.

| Assumed paired SD at 1 tick/lot | Pairs for 80% | Pairs for 90% | Approx. power at 400 |
|---|---:|---:|---:|
| Pilot SD | 249 | 313 | 96% |
| 1.25 × pilot SD | 389 | 489 | 81% |
| 1.5 × pilot SD | 560 | 705 | 61% |

At half the effect and the same pilot SD, the estimates rise to 995 and
1,252 pairs. Pilot variance and endpoint volume are uncertain. Check power
under plausible symmetric heavy-tailed alternatives before launching and
fix the sample size, stopping rule, and analysis before inspecting follow-up
outcomes. Use 200,000 seeded sign assignments with the existing plus-one
Monte Carlo procedure and Holm correction.

The following fixed rule generates the 400 proposed IDs in order. Freeze
the resulting list in the follow-up configuration and exclude pilot seeds
2026–2033 from confirmation.

```python
from hashlib import sha256

seeds = [int.from_bytes(sha256(f"arbiter-x/flagship-confirmatory-v1/{i}".encode("ascii"))
                       .digest()[:8], "big") for i in range(400)]
assert len(set(seeds)) == 400 and set(seeds).isdisjoint(range(2026, 2034))
print("seeds = " + ",".join(map(str, seeds)))
```

Methods follow [Holm's family-wise correction](https://www.jstor.org/stable/4615733)
and [Phipson and Smyth's random-permutation p-values](https://arxiv.org/abs/1603.05766).

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

The pilot's 5,000-repetition setting applies only to the Monte Carlo path.
Its results use exact enumeration.
