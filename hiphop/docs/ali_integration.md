# Ali Integration Notes

Production preset for the Hip-Hop handoff v2 is `config/hiphop/final_v2.json`.

Use `docs/integration_guide_for_ali.md` as the integration document. It covers the library call, quiet/open processing, hot/dense protection, the −1.05 dBTP ceiling, and why the chain does not force a single LUFS target.

Do not load `config/hiphop/default.json` or `config/hiphop/final_approved.json` for new production jobs. `final_approved.json` remains the previous release.
