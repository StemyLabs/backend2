# Integration Guide for Ali

STEMY/Ali owns backend, frontend, storage, billing, and deployment. This document covers how to build and invoke the Hip-Hop DSP.

Production preset: `config/hiphop/final_v2.json` (`hiphop-final-v2`).

Do not point production jobs at `final_approved.json`. That file is the previous release and is unchanged.

## INPUT

| Constraint | Requirement |
|------------|-------------|
| Container | WAV |
| Channels | Stereo only. Mono and more than 2 channels are rejected. |
| Sample rates | 44100, 48000, 88200, 96000 |
| Samples | Finite, non-empty |

MP3, AAC, and other codecs must be converted to WAV before this library.

## INVOCATION

Link `stemy::dsp`. Do not shell out to the CLI in the service path.

```cpp
#include "stemy/config/config_loader.h"
#include "stemy/mastering/mastering_engine.h"

stemy::genres::hiphop::HipHopParameters params;
auto st = stemy::config::load_hiphop_config(
    "config/hiphop/final_v2.json", params);
if (!st) { /* handle st.message() */ }

stemy::mastering::MasteringRequest req;
req.input_path = input_wav;
req.output_path = output_wav;
req.genre_params = params;

stemy::mastering::MasteringEngine engine;
stemy::mastering::MasteringStats stats;
st = engine.process_file(req, stats);
if (!st) { /* fail the job; log stats and st.message() */ }
```

In-memory path: `engine.process_buffer(buffer, format, params, stats)`.

CLI for QA:

```bash
./build/tools/stemy_master/stemy_master in.wav out.wav \
  --config config/hiphop/final_v2.json \
  --report-json report.json
```

Create one `MasteringEngine` per job. Do not share a chain across threads.

## Adaptive behavior

The chain measures integrated loudness, crest, true peak, spectrum, and stereo image, then builds one deterministic decision.

Quiet/open material (not hot, crest at least 12 dB, and either below −14 LUFS or below the −9 LUFS band edge) may receive:

- A clean-loudness search in +0.25 dB steps, up to +7 dB above the baseline gain. The search stops at about −9 LUFS or at the first stress-gate failure, whichever comes first.
- Open-mix saturation at drive 1.50 / mix 0.22.
- A pre-limiter soft-peak rounder (ceiling +5.5 dBFS, drive 1.4, 4× oversampling).

Hot or dense material does not enter that search. Punch and saturation stay off, stereo width stays 1.0, and the soft-peak stage stays off. Antonio-class hot/dense inputs are the regression check for this bypass.

Stress gates are unchanged from the previous release: limiter p95 gain reduction ≤ 7.5 dB, average active gain reduction ≤ 2.0 dB, time above 6 dB gain reduction ≤ 10%, max gain reduction ≤ 12 dB, output crest ≥ 10.5 dB, and true peak within the ceiling.

## True peak

The engineering ceiling is −1.05 dBTP. The limiter is inter-sample aware (4×). A final true-peak safety stage remains after the limiter. A master that measures above the ceiling fails the job.

## Why there is no fixed LUFS target

The −9 to −8 LUFS band and the −9 LUFS search stop are guidance, not a mandate. A quiet/open song keeps the last candidate that still passes the stress gates, even if that result is quieter than −9 LUFS. A hot song is turned down toward the band instead of being pushed louder. Forcing one LUFS number would override punch, clarity, and low-end control on some songs. Do not add a post-process loudness normalizer to hit a single number.

## OUTPUT

| Output | Description |
|--------|-------------|
| WAV | Stereo, same sample rate as the input |
| `MasteringStats` | Input analysis, output analysis, decision notes, safety report |
| `Status` | Ok, or Error with a message |

Safety fields include limiter gain-reduction stats, true-peak safety, and NaN/clip counts. For 24-bit review files, `stemy_export_review` applies TPDF dither only and does not change the DSP.

## Resources

Offline, full-file processing. No GPU and no model. Quiet/open jobs may render multiple candidates (at most 28 extra steps). Size job timeouts for that. Memory is on the order of the decoded stereo buffer.

Website, auth, billing, storage, and UI are out of scope.
