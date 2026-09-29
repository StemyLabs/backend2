# IMMUTABLE M2B Baseline Freeze

**Freeze ID:** `m2b-frozen`  
**Branch:** `hiphop-final-phase`  
**Canonical config:** `config/hiphop/m2b_frozen.json`  
**Sonic policy source:** `m2-refinement-v2` (`make_m2_refinement_defaults()`)

## What is frozen

- Adaptive loudness (M1 baseline floor + open-premix extra)
- Punch / saturation / HF / width character stages
- Adaptive low-end weight
- True-peak ceiling **-1.0 dBTP**
- Compressor OFF
- Character-first limiter-stress backoff

## Rules for final Hip-Hop phase

1. Do **not** edit `config/hiphop/m2b_frozen.json` for sonic changes.
2. Do **not** change M1 (`config/hiphop/default.json`).
3. Final-phase experiments must use a **new** versioned config (e.g. `m3-final-candidate.json`).
4. This freeze exists to validate unchanged M2B on new client mixes before any retune.

## Tag

Intended git tag: `m2b-frozen` (created after freeze commit of this baseline).
