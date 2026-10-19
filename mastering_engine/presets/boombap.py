"""
Boom Bap preset — punchy kick/snare transients, warm low-end, authentic analog glue.
Connected to Xavier's C++ JUCE mastering engine.
"""

BOOMBAP = {
    "label": "Boom Bap",
    "hpf_hz": 24.0,
    "low_shelf":   {"freq_hz": 80.0,   "gain_db": 4.5},
    "mid_dip":     {"freq_hz": 400.0,  "q": 1.0,  "gain_db": -2.0},
    "presence":    {"freq_hz": 2800.0, "q": 0.8,  "gain_db": 1.5},
    "air_shelf":   {"freq_hz": 9000.0, "gain_db": 1.2},
    "saturation_drive": 0.22,
    "comp": {
        "threshold_db": -19.0,
        "ratio": 3.0,
        "attack_ms": 15.0,
        "release_ms": 90.0,
        "knee_db": 8.0,
        "makeup_db": 2.0,
    },
    "width": 1.35,
    "limiter": {
        "threshold_db": -0.8,
        "release_ms": 70.0,
    },
    "target_lufs": -8.5,
    "target_tp_db": -0.5,
}
