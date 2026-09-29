# STEMY Hip-Hop DSP — Final Handoff V2

**Release marker:** `STEMY_HIPHOP_FINAL_HANDOFF_V2`  
**Release date:** 2026-09-23  
**Production config:** `config/hiphop/final_v2.json`  
**Config version:** `hiphop-final-v2`  
**Build version:** `stemy_dsp` 0.1.0  

Offline, deterministic stereo Hip-Hop mastering. Not a machine-learning system.

`config/hiphop/final_approved.json` is the previous release. It is unchanged. Production jobs use `final_v2.json`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

./build/tools/stemy_master/stemy_master input.wav output.wav \
  --config config/hiphop/final_v2.json \
  --report-json report.json
```

Details: `docs/build_instructions.md` and `docs/integration_guide_for_ali.md`.

## What the chain does

It measures the premaster, then applies one adaptive Hip-Hop chain: input gain, tonal and low-end control, optional punch, saturation, high-frequency openness, a small stereo width, a pre-limiter soft-peak stage when eligible, an inter-sample limiter, and true-peak safety.

Suitable quiet/open mixes can search louder in small steps, use a denser saturation setting, and receive adaptive peak management before the final limiter. The search stops when the material reaches the loudness guidance region or when a stress gate fails.

Hot or dense mixes stay protected. They are not given the extra loudness search, the open-mix saturation, the punch stage, or the soft-peak stage.

The true-peak ceiling remains −1.05 dBTP.

## What the chain does not do

- It does not force every song to one LUFS target.
- It does not raise the true-peak ceiling or relax the limiter stress gates.
- It does not apply broad compression. The compressor stays off.
- It does not contain song-specific processing.

The loudness band is guidance. Quiet/open songs keep the last candidate that still passes the gates, which may be quieter than −9 LUFS. Hot songs are reduced toward the band instead of being pushed to a number. A single forced target would give up punch, clarity, or low-end control on some material.

## Layout

| Path | Contents |
|------|----------|
| `include/`, `src/` | DSP library |
| `config/hiphop/final_v2.json` | Production preset |
| `tools/` | CLI |
| `tests/` | Automated tests |
| `third_party/` | pffft, dr_wav, nlohmann/json, GoogleTest |
| `docs/` | Build and Ali integration notes |

Proprietary STEMY code. Third-party components keep their upstream licenses.
