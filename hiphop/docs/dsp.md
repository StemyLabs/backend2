# DSP modules (Milestone 1)

All processors are **parameter-driven** and genre-agnostic.
Default / adaptive decisions keep them **transparent**.

## Modules

| Module | Transparent condition | Notes |
|--------|----------------------|-------|
| `GainStage` | `gain_db == 0` | Safety-clamped gain range |
| `Eq` | no enabled bands / 0 dB | Up to 8 biquad bands |
| `LowEndControl` | `enabled == false` | Dedicated HPF / low shelf / bass-mono |
| `Compressor` | disabled or `ratio == 1` | Feed-forward peak detector |
| `Saturation` | disabled / `drive==0` / `mix==0` | Provisional tanh curve only |
| `StereoProcessor` | disabled or `width==1` | M/S width |
| `Limiter` | disabled | Lookahead peak limiter + hard ceiling |

## Sonic policy

- Do **not** invent final EQ curves, saturation character, compressor settings,
  stereo widening, or loudness targets.
- Saturation’s tanh shape is a **placeholder nonlinearity** for architecture/tests.
- Limiter ceiling in config is **provisional** and off by default.

## Low-end control

Separate from general EQ so bass management can evolve independently
(conservative HPF, optional shelf, optional mono-below frequency).

## Safety helpers

`apply_output_safety` detects NaN/Inf, optional replacement, denormal flush,
and clip reporting.
