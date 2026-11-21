# Reproducibility

What "reproducible" means here, exactly.

## Determinism contract (same arch/build)

For a fixed code revision/toolchain/build, identical configuration, seed and
initial state produce identical execution results.
Covered by: `arbiter verify` (IDENTICAL), `test_world` determinism,
snapshot restore contracts, artifact reproduce commands. NOT claimed:
cross-architecture / cross-compiler bitwise equality (see DETERMINISM.md).

## State-hash guarantee (`ARBITERX/WORLD/3`)

FNV-1a 64 over a documented serialization (fixed order, sorted maps/sets).
Covers all EVENT-RELEVANT state: clock, progress counters, book ladder +
order-account ownership, full strategy state (view ladder, queue-ahead,
confirm_seq), per-account risk, sim dynamics + RNG progress, channel RNG +
burst + corruption stream, feed progress + pending buffer, retransmit-ring
logical contents, gateway ids, in-flight frames, markout probes, scheduler
contents. Excludes: journal bytes, RunStats histograms/timelines, wall-clock,
latency-tracing tables (`order_sent_t_`/`stages_`, histogram inputs only),
config identity (lives in `config_hash()`/`world_id()` so fork-divergence
assertions stay meaningful). Non-cryptographic: equality is evidence, and
differential tests compare full views, not just hashes.

## Config identity (`ARBITERX/WORLDCFG/3`)

Every behavior-changing field is hashed (explicit per-field test:
`config_hash_covers_all_behavior_fields`); instrumentation-only fields
(journal path/toggles, checkpoint/hash cadence) are excluded by design. Any
new behavior field MUST extend the hash and bump the tag.

## Network RNG streams (fixed-draw contract)

`Channel::next_delivery` consumes exactly 7 draws per message in fixed order
(loss, burst-arm, dup, reorder-decide, reorder-magnitude, corrupt, jitter),
including drops and mid-burst messages. Profiles gate interpretation, never
alignment, varying one network variable cannot shift unrelated randomness
(tested: loss patterns invariant to jitter/dup/corrupt settings).

## Provenance and retained evidence

The retained JSON and Markdown files are generated together from a fresh
Release build of a clean committed checkout. The JSON records the producer's
full Git revision, source state, compiler, platform and UTC generation time.
Check out the producer revision in a separate worktree and pass a new output
directory outside that checkout:

```sh
git worktree add --detach /path/to/clean-arbiter <core_revision>
/path/to/clean-arbiter/scripts/regenerate_flagship.sh /path/to/new-output
```

The script checks the checkout and build's source state, generates the three
pilot variants and checks their outputs. Move the generated files into the
publishing checkout without editing them. The producer revision precedes the
commit that adds the generated artifacts.

`arbiter flagship --reproduce results.json --report report.md` recomputes and
compares canonical JSON and Markdown bytes. It carries forward the original
producer metadata, including the UTC timestamp, as part of that comparison.
Establish provenance separately by checking the clean producing revision and
build. For an older artifact schema, use the recorded generating revision.
Historical EXP records live in ARBITER-X-LAB.
