# Handoff Manifest — STEMY Hip-Hop Final V2

| Field | Value |
|-------|-------|
| Release marker | `STEMY_HIPHOP_FINAL_HANDOFF_V2` |
| Release date | 2026-09-23 |
| Production config | `config/hiphop/final_v2.json` |
| Config version | `hiphop-final-v2` |
| Build version | `stemy_dsp` 0.1.0 |
| Previous production config | `config/hiphop/final_approved.json` (unchanged; do not use as the v2 preset) |
| Previous release tag | `STEMY_HIPHOP_FINAL_HANDOFF` / `hiphop-final-approved` |

## What Ali should call

```bash
./build/tools/stemy_master/stemy_master input.wav output.wav \
  --config config/hiphop/final_v2.json \
  --report-json report.json
```

## Not in this release

- R2 / R2B / R2C experimental configs and candidate matrices
- Bars-only listening renders
- The previous handoff package (`STEMY_HIPHOP_FINAL_HANDOFF`). That package remains the prior approved release.

`final_approved.json` is still in the tree so the previous preset can be identified. v2 does not replace that file.

## Documentation

| Document | Purpose |
|----------|---------|
| `README.md` | What the chain does and does not do |
| `docs/integration_guide_for_ali.md` | Library and CLI integration |
| `docs/build_instructions.md` | Clean build |
| `docs/architecture.md` | Pipeline |
| `docs/adaptive_logic.md` | Present in the prior package; v2 behavior is summarized in the README and Ali guide |
