# Build Instructions

Verified on Linux with GCC 11.4 and CMake 3.20+.

## Requirements

| Item | Requirement |
|------|-------------|
| CMake | ≥ 3.20 |
| Compiler | C++20 (GCC 11+ or Clang 14+) |
| Network | Not required. GoogleTest is vendored under `third_party/googletest`. |

## Configure, build, test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Library and tools only:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTEMY_BUILD_TESTS=OFF
```

## Production run

```bash
./build/tools/stemy_master/stemy_master input.wav output.wav \
  --config config/hiphop/final_v2.json \
  --report-json report.json
```
