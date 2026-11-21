# Simulation

## What the market generator IS

The `MarketSim` bot population generates provider and taker flow:

* **Provider bots** (`provider_arrival_per_ms`, 3 accounts): Poisson
  arrivals; join inside the spread at the observed touch (sometimes
  improving the quote by extra ticks). Random cancels (`cancel_fraction`) target resting provider orders.
* **Taker bots** (`taker_arrival_per_ms`): Poisson arrivals of aggressive
  IOC orders with a momentum bias from recent trade directions
  (`0.5 + 0.08 * bias`, bias clamped ±4).
* **Regime chain** (`regime_dwell_ns`): a two-state calm/volatile Markov
  chain scaling arrival rates (×3) and sizes (×2). Dwell 4ms default.
* **Taker self-excitation** (`taker_self_excitation`, k in [0,1), default
  0 = off): this is the clustering parameter in the flagship study.
  Two coupled mechanisms, both armed by the same k:
  1. *intensity memory*: each executed trade shortens the NEXT taker
     inter-arrival by factor (1-k) (one-shot boost; no extra RNG draw);
  2. *sign persistence*: a taker follows the previous taker's side with
     probability k.
  k = 0 uses the memoryless taker process within each regime. The one-shot
  boost and sign persistence give a seedable, snapshot-safe clustered control.

## Measured regime properties (seed 99, 300ms, simstats)

| quantity | k=0 (Poisson null) | k=0.9 (clustered) |
|---|---|---|
| trades | 6,762 (22.5k/s) | 32,034 (106.8k/s) |
| trade-sign persistence (repeat rate) | 0.000 (perfect alternation) | 0.416 |
| trade inter-arrival burstiness (cv) | 1.71 (regime chain alone) | 2.55 |
| mid lag-1 autocorr @ trade times | ~0.0 | ~1.0 |

Calibration facts (simulator properties, NOT claims of
realism):

* In these default k=0 runs, the market alternates tick-for-tick: provider replenishment
  (60/ms) dominates taker pressure (8/ms), so sign runs never emerge and
  the momentum archetype never trades. This was the historical behavior.
* The retained clustered study uses k=0.9, which produces sign signals
  that activate the momentum strategy under its recorded seeds. This is a
  model choice, not a calibrated threshold.
* k=0 still shows inter-arrival cv > 1, the calm/volatile regime chain is
  itself a clustering mechanism (rate persistence over 4ms dwell times).
  The Poisson-null claim is about the TAKER PROCESS given the regime, not
  about the absence of any clustering anywhere.

## Bots and the strategies

Bots are co-located with the exchange: their orders skip the network and
the risk gateway (documented asymmetry). They interact with the strategies
through the real book, and, in the default reactive mode, they OBSERVE the
live touch (including strategy quotes). The frozen mode
(`bots_observe_shadow`) removes the observation channel's strategy
dependence (docs/COUNTERFACTUALS.md).

## Not claimed

The simulator models stylized depth and homogeneous sizes within each bot
class. The median spread in the runs above was 1 tick. `simstats` measures
spread for each run.
