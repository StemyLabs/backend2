# M1 Client Validation Brief

**Status:** Engineering Gate A complete. Sonic validation (Gate B) pending.  
**Config:** `m1-master01-baseline` (`config/hiphop/default.json`)  
**Outputs:** `refs/evaluation_outputs/m1/`

These outputs are a **working Hip-Hop prototype** for first-round listening.
They are **not** commercially final masters.

---

## 1. What M1 implements

- Stereo WAV ingest with validation (stereo-only; 44.1/48/88.2/96 kHz)
- Analysis: LUFS-I/ST, sample peak, oversampled true peak, RMS, crest, spectral bands, stereo correlation/width
- Adaptive control for **quiet / in-pocket / hot** inputs toward a nominal ≈ **−9 LUFS-I** reference
- Sample-peak limiter (when enabled by adaptive) with ceiling policy **−0.5 dBTP**
- Final **oversampled true-peak safety** (ceiling is a **maximum**, not a loudness target)
- Quiet-input mild tonal/low-end trim (MASTER_01 *direction* only)
- Tight stereo policy (no widening)
- Safety: NaN/Inf handling, channel-balance check, determinism tests
- CLI + JSON metric reports for QA

## 2. What M1 intentionally does **not** implement

- Compressor / densification beyond gain + limiting
- Saturation / harmonic exciters
- Stereo widening or creative M/S imaging
- Exact matching of `STEMY_MASTER_01` on every title
- Final commercial EQ curves or genre “polish”
- ML / generative processing
- Claiming streaming-competitive “finished master” quality

## 3. Test tracks processed

| Track | Premaster | Output |
|-------|-----------|--------|
| Despo | `refs/stemy_premasters/00Despo - ONE.wav` | `Despo_ONE_m1.wav` |
| NY Energy | `refs/stemy_premasters/New York Energy premix.wav` | `NY_Energy_m1.wav` |
| LADI ROCK | `refs/stemy_premasters/OBSESSED - LADI ROCK.wav` | `LADI_ROCK_m1.wav` |
| Tef | `refs/stemy_premasters/Tef - Don't Make Me.wav` | `Tef_Dont_Make_Me_m1.wav` |

Metric aggregate: `refs/evaluation_outputs/m1/m1_premaster_metrics.json`

## 4. Objective measurements (summary)

| Track | In LUFS-I | Out LUFS-I | In TP | Out TP | In crest | Out crest | Gain | Path |
|-------|-----------|------------|-------|--------|----------|-----------|------|------|
| Despo | −6.35 | **−9.00** | −0.53 | **−3.17** | 7.93 | 7.93 | **−2.65 dB** | Hot reduce |
| NY Energy | −14.56 | −11.15 | −0.64 | **−0.50** | 14.88 | 11.83 | **+5.56 dB** | Quiet lift + limit |
| LADI ROCK | −18.82 | −10.25 | −2.08 | **−0.50** | 19.84 | 12.59 | **+9.82 dB** | Quiet lift + limit |
| Tef | −5.59 | **−9.00** | −0.11 | **−3.52** | 7.77 | 7.77 | **−3.41 dB** | Hot reduce |

- **In loudness pocket (±0.75 LU of −9):** Despo, Tef  
- **Below pocket after lift+limit:** NY Energy, LADI ROCK (limiter prevented full −9 while holding −0.5 dBTP)  
- **True-peak safety stage:** did **not** apply extra attenuation on these four (`true_peak_safety_applied=false`)  
- **Stereo processor:** width fixed at 1.0 (no widening)  
- **Quiet-path EQ/low-end:** NY Energy + LADI ROCK only  
- **Hot path tonal:** none on Despo + Tef  

### Why Despo / Tef sit well below −0.5 dBTP

The −0.5 dBTP value is a **ceiling** (do not exceed), not a target to “fill up to.”

Both tracks were **already hotter than −9 LUFS-I**. Adaptive control applied **uniform gain reduction** to reach −9 LUFS-I. That same attenuation also lowered true peak by roughly the gain amount:

- Despo: −0.53 dBTP + (−2.65 dB) ≈ **−3.18 dBTP**  
- Tef: −0.11 dBTP + (−3.41 dB) ≈ **−3.52 dBTP**

After that, peaks were already under the ceiling, so the limiter / true-peak safety had nothing further to pull down—and M1 does **not** boost peaks up to −0.5.

## 5. Known limitations

- No compressor yet → limited ability to raise loudness while holding −0.5 dBTP on open premasters (NY / LADI)
- Quiet-path EQ is provisional / mild, not a finished commercial curve
- True-peak estimator uses provisional oversampled linear interpolation
- C++ path is WAV-only
- Prototype prioritizes adaptive architecture over final commercial polish

## 6. Questions for client listening

Please A/B each premaster vs its `*_m1.wav` output and note:

1. **Low-end weight** — too light, right, or still muddy/boomy?  
2. **Kick / 808 punch** — better, worse, or unchanged vs premaster?  
3. **Vocal presence** — clearer, buried, or harsh?  
4. **High-end clarity** — dull, balanced, or brittle?  
5. **Warmth** — missing, appropriate, or excessive?  
6. **Stereo image** — too narrow, fine, or wanting width?  
7. **Overall loudness** — competitive enough for this stage, or needs more density?  
8. **Overall commercial direction** — closer to the MASTER_01 “pocket,” or off?

Optional reference context (not a hard match target): client-preferred `STEMY_MASTER_01` from the Bumpman evaluation.

Please return track-by-track notes. Those notes—not further unapproved DSP invention—will drive the next explicit parameter decisions.
