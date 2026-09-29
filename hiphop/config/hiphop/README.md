# Hip-Hop configuration (Milestone 1)

## Baseline

`default.json` encodes **provisional** targets derived from the client-preferred
`STEMY_MASTER_01` result on the Bumpman Bandwagon eval.

T.I. – About The Money is commercial **context**, not a hard match target.

## Encoded targets (provisional)

| Target | Value | Source |
|--------|-------|--------|
| Integrated LUFS | −9.0 (±0.75) | MASTER_01 ≈ −8.9 |
| True-peak ceiling | −0.5 dBTP | MASTER_01 ≈ −0.5 (not 0) |
| Stereo | width = 1.0 (no widen) | MASTER_01 stayed tight |
| Quiet tonal trim | mild sub↓ / bass·low-mid↑ / top↓ | MASTER_01 vs premaster direction |
| Crest | ~10.2 dB contextual | not a direct compressor target yet |

## Adaptive behavior

- **Quiet input** (< −14 LUFS): lift toward −9, enable −0.5 limiter, optional mild tonal trim
- **In pocket**: near-unity gain
- **Hotter than pocket**: reduce toward −9 (capped), do not push louder
- Saturation / compressor: still bypassed until explicitly approved

Do not treat EQ magnitudes as final commercial curves.
