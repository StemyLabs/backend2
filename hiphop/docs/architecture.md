# STEMY DSP Architecture (Milestone 1)

## Purpose

STEMY Milestone 1 delivers a production-oriented, deterministic DSP foundation
for an automated stereo mastering engine. It is **not** an ML/generative system.

Hip-Hop adaptive defaults follow the **STEMY_MASTER_01** evaluation baseline
(Bumpman premaster → client-preferred master), with T.I. as commercial context
only. Parameters remain provisional and configurable via `config/hiphop/`.

## Module responsibilities

| Module | Responsibility |
|--------|----------------|
| `audio` | `AudioBuffer`, `AudioFormat`, WAV I/O, input validation |
| `fft` | Internal FFT abstraction (pffft backend, swappable) |
| `analysis` | Measurement only → `AnalysisResult` |
| `adaptive` | Maps analysis + genre config → `ProcessingDecision` |
| `dsp` | Parameter-driven processors (no genre knowledge) |
| `genres/hiphop` | Hip-Hop chain + provisional parameters |
| `config` | JSON loading (offline / CLI; not in sample loop) |
| `mastering` | Orchestrates full pipeline |
| `tools/stemy_master` | CLI only |

## Processing flow

```
Input WAV
  → Audio validation (stereo-only, allowed sample rates, finite samples)
  → Input analysis → AnalysisResult
  → AdaptiveEngine(AnalysisResult, HipHopParameters) → ProcessingDecision
  → HipHopChain stages:
       1. Input gain/headroom
       2. EQ
       3. Low-end control
       4. Dynamics
       5. Saturation
       6. Stereo processing
       7. Limiting
       8. Output safety
  → Output analysis → AnalysisResult
  → Output WAV
```

## Separation rules

1. **Measurement ≠ decision ≠ processing**
2. Generic DSP modules never contain Hip-Hop (or other genre) targets
3. Genre behavior lives in `genres/` + `config/`
4. JSON config is parsed outside real-time DSP code paths

## Adding a new genre later

1. Add `include/stemy/genres/<genre>/` parameters + chain
2. Add `config/<genre>/`
3. Optionally specialize `AdaptiveEngine` rules (or add a genre strategy)
4. Keep `dsp::*` unchanged unless a new processor type is required

## Backend integration (Ali / STEMY service)

Recommended integration path:

1. Link the static library `stemy_dsp` into the backend worker
2. Call `stemy::mastering::MasteringEngine::process_file` or `process_buffer`
3. Pass genre config loaded via `stemy::config::load_hiphop_config` (or in-memory `HipHopParameters`)
4. Persist `MasteringStats` (input/output analysis + decision notes) for QA
5. Keep the CLI as a local/dev tool; do not embed CLI parsing in the service

Thread model (M1): instances are not claimed realtime-safe for shared mutable
use across threads — create per-job engine/chain instances.

## Safety

The pipeline rejects invalid channel counts (non-stereo), unsupported sample
rates, and non-finite input. Output safety detects/replaces NaN/Inf, flushes
denormals, and reports clipping.
