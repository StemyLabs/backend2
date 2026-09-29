# Milestone 1 Acceptance Checklist

**Title:** Adaptive DSP Foundation + Initial Hip-Hop Prototype  
**Status date:** 2026-08-25  
**Engineering status (Gate A):** **COMPLETE** (objective evidence below)  
**Sonic validation status (Gate B):** **PENDING** (client first-round listening)

## Scope (agreed)

- Shared audio analysis framework
- Adaptive DSP control architecture
- Initial Hip-Hop mastering chain
- Initial parameter ranges and processing behavior
- Working Hip-Hop prototype for first-round testing

## Non-goals for M1

- Final commercial sonic tuning / Gate B listening sign-off
- Matching `STEMY_MASTER_01` exactly on every input
- ML / generative AI
- Plugin formats / GUI
- Unapproved saturation, compression, or stereo widening

## Baseline policy (provisional)

`STEMY_MASTER_01` is the client-preferred sonic **baseline**, not a universal hard target.

| Parameter | Provisional value |
|-----------|-------------------|
| Integrated loudness reference | ≈ −9 LUFS-I (adaptive band) |
| True-peak ceiling | −0.5 dBTP |
| Stereo widening | Off |
| Saturation / compressor | Off |

---

## Checklist legend

| Mark | Meaning |
|------|---------|
| `[x]` | Implemented and evidenced |
| `[~]` | Accepted limitation (documented) |
| `[ ]` | Missing |

---

## 1. Audio input validation

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 1.1 | Stereo only; mono rejected | `[x]` | `src/audio/wav_io.cpp`, `Validation.RejectsMono` |
| 1.2 | >2 channels rejected | `[x]` | `validate_input_audio` |
| 1.3 | SR 44.1/48/88.2/96 | `[x]` | `audio_format.h`, `Validation.RejectsBadSampleRate` |
| 1.4 | Bad SR fails safely | `[x]` | same |
| 1.5 | Bad file errors | `[x]` | `read_wav` / CLI |
| 1.6 | Non-finite input rejected | `[x]` | `AudioBuffer::validate_finite` |
| 1.7 | Formats documented | `[x]` | `docs/ali_integration.md` — **WAV only** in C++ |

**Section:** Complete.

---

## 2. Analysis framework

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 2.1–2.10 | LUFS-I/ST, peaks, RMS, crest, spectral bands, stereo | `[x]` | `src/analysis/*`, `AnalysisPipeline` |
| 2.11 | CLI / report exposes metrics incl. spectral | `[x]` | CLI print + `--report-json` |
| 2.12 | Analyzer unit tests | `[x]` | `analyzers_test.cpp`, `stereo_analyzer_test.cpp` |
| 2.13 | LUFS absolute golden vectors | `[~]` | BS.1770 implemented; absolute ITU test vectors not checked in (accepted M1 limitation) |
| 2.14 | TP method | `[~]` | Shared oversampled linear-interp measure (`true_peak_measure.*`); provisional, documented |

**Section:** Complete for M1 with documented measurement limitations.

---

## 3. Adaptive control

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 3.1 Quiet lift | `[x]` | `Adaptive.QuietInputLiftsTowardMaster01Pocket` |
| 3.2 In-pocket | `[x]` | `Adaptive.InPocketNearUnityGain` |
| 3.3 Hot no-boost | `[x]` | `Adaptive.HotInputDoesNotBoost` |
| 3.4 Safe ranges | `[x]` | `HipHopParameters`, JSON |
| 3.5 No gain escalation | `[x]` | `max_lift_db` / `max_reduction_db` |
| 3.6 Not hard MASTER_01 clone | `[x]` | tolerance + path branching |
| 3.7 Decision notes | `[x]` | `decision_notes` + reports |
| 3.8 JSON configurable | `[x]` | `config/hiphop/default.json` |

**Section:** Complete.

---

## 4. Hip-Hop prototype

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 4.1 Configurable chain | `[x]` | `hiphop_chain.*` |
| 4.2 Loudness control | `[x]` | adaptive gain |
| 4.3 True-peak ≤ −0.5 | `[x]` | `TruePeakSafety` + mastering hard check + premaster reports |
| 4.4 Quiet tonal/low-end | `[x]` | approved quiet-path trim only |
| 4.5 Saturation OFF | `[x]` | adaptive forces off |
| 4.6 Compressor OFF | `[x]` | adaptive forces off |
| 4.7 No widening | `[x]` | width=1.0 |
| 4.8 Working executable | `[x]` | `stemy_master` |

**Section:** Complete.

---

## 5. Safety

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 5.1 NaN/Inf | `[x]` | input + `apply_output_safety` |
| 5.2 Clipping reported | `[x]` | `SafetyReport` |
| 5.3 Sample-peak limiter | `[x]` | `Limiter` when enabled |
| 5.4 Output TP ≤ −0.5 dBTP | `[x]` | `TruePeakSafety` + `MasteringEngine` fail-closed + tests + premaster set |
| 5.5 Channel balance | `[x]` | relative Δ ≤ 0.25 dB; `ChannelBalance.*` tests |
| 5.6 Determinism | `[x]` | `Regression.SameInputSameConfigDeterministic` (tol **1e-6**) |

**Section:** Complete.

---

## 6. Testing

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 6.1 Unit tests | `[x]` | 36 CTest cases (2026-08-25) |
| 6.2 Analysis tests | `[x]` | analyzers + stereo |
| 6.3 Adaptive/processing | `[x]` | adaptive + hiphop + TP safety |
| 6.4 E2E | `[x]` | `E2E.ProcessSineWavRoundTrip` |
| 6.5 Regression/determinism | `[x]` | `tests/regression/determinism_test.cpp` |
| 6.6 Premaster set under baseline | `[x]` | `refs/evaluation_outputs/m1/*` + `m1_premaster_metrics.json` |

### Premaster deliverable set (Gate A)

| Track | Output | Out TP dBTP | TP ≤ −0.5 |
|-------|--------|-------------|-----------|
| Despo | `Despo_ONE_m1.wav` | −3.17 | yes |
| NY Energy | `NY_Energy_m1.wav` | −0.50 | yes |
| LADI ROCK | `LADI_ROCK_m1.wav` | −0.50 | yes |
| Tef | `Tef_Dont_Make_Me_m1.wav` | −3.52 | yes |

Batch script: `scripts/batch_m1_premasters.sh`

**Section:** Complete.

---

## 7. Deliverables

| ID | Criterion | Status | Evidence |
|----|-----------|--------|----------|
| 7.1 Source | `[x]` | `include/`, `src/`, `tools/` |
| 7.2 CMake | `[x]` | root `CMakeLists.txt` |
| 7.3 Config | `[x]` | `config/hiphop/default.json` |
| 7.4 Tests | `[x]` | `tests/` (orphan stereo test wired; obsolete `test_signals.cpp` removed) |
| 7.5 CLI | `[x]` | `stemy_master` + `--report-json` |
| 7.6 Prototype outputs | `[x]` | `refs/evaluation_outputs/m1/` |
| 7.7 Tech docs | `[x]` | `docs/architecture.md`, `analysis.md`, `dsp.md` |
| 7.8 Parameter docs | `[x]` | `docs/parameter_reference.md` |
| 7.9 Ali integration | `[x]` | `docs/ali_integration.md` |

**Section:** Complete.

---

## Dual completion gates

| Gate | Status |
|------|--------|
| **A. Engineering complete** | **YES** — all required criteria have objective evidence |
| **B. Sonic validation** | **PENDING** — client listening on `refs/evaluation_outputs/m1/` |

### Accepted engineering limitations (do not reopen Gate A)

- True-peak estimator uses provisional linear-interp oversampling (shared by analysis + safety).
- Absolute ITU LUFS test vectors not checked in.
- C++ path is WAV-only.

### Explicitly still out of scope until approved

- Compressor / saturation / stereo widening  
- New EQ curves beyond quiet-path trim  
- Declaring commercial-final Hip-Hop sound (Gate B)
