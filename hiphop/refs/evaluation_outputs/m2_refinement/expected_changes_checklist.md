# M2 Refinement — Expected Changes Checklist (pre-implementation)

Conservative refinement vs corrected M2 checkpoint. M1 frozen.

| Track | TP | Loudness target | Punch | Sat / warmth | Low-end weight | Other |
|---|---|---|---|---|---|---|
| **NY Energy** | −0.5 → **−1.0 dBTP** | −10.65 → **~−9.2…−9.0 LUFS** (not forced to −8.5) | Moderate ↑ (kick-focused) | Subtle ↑ | Unlikely (bass share OK) | Less limiter squash; modest extra lift on open premix |
| **LADI ROCK** | **−1.0 dBTP** | −9.85 → **~−9.3…−9.0** | Moderate ↑ | Subtle ↑ | **On** (sparse bass body) | HF stays bypassed if bright; width unchanged |
| **Holiday Hustle** | **−1.0 dBTP** | −9.86 → **~−9.3…−9.0** if safe | Moderate ↑ | Subtle ↑ (conservative on overs) | **On** | Source overs warning retained |
| **Perfect Timing** | **−1.0 dBTP** | −8.74 → **~−8.7…−8.5** | Moderate ↑ | Subtle ↑ | Off (bass sufficient) | Width bypass; no forced −9 |
| **Despo ONE** | **−1.0 dBTP** | ~−8.5 (hot path) | **Off** (dense crest) | **Off** | Off (bass dominant) | Punch/sat bypass unchanged |
| **Tef** | **−1.0 dBTP** | ~−8.5 (hot path) | **Off** (dense crest) | **Off** | Off | Same as Despo |

**Regression guard:** no quiet track >0.5 LU below M1 without documented override.
