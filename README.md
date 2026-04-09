# Third Person Shooter Engine

A lightweight, modular C++ third-person shooter engine prototype focused on deterministic updates and practical runtime optimization.

## Highlights
- Deterministic fixed-step simulation (`60 Hz`) with frame-time clamping.
- Lean engine loop with reduced runtime overhead and profiler aggregation.
- Modular subsystems (`core`, `graphics`, `physics`, `input`) with coherent interfaces.
- Contiguous game-object storage for better cache behavior (`std::vector<Enemy>`).
- CI-ready pipeline with Debug/Release builds in GitHub Actions.

## Project Structure
```text
third-person-shooter-engine/
├── engine/
│   ├── core/
│   ├── graphics/
│   ├── input/
│   ├── physics/
│   └── optimization/
├── game/
│   ├── enemies/
│   └── player/
├── assets/
├── include/
├── .github/workflows/
├── CMakeLists.txt
└── README.md
```

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Strict Build (Pre-Push)
```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Debug -DTPS_ENGINE_WARNINGS_AS_ERRORS=ON
cmake --build build-strict --parallel
```

## Run
```bash
./build/third_person_shooter
```

## CMake Options
- `TPS_ENGINE_ENABLE_IPO` (default: `ON`): enables IPO/LTO when supported.
- `TPS_ENGINE_ENABLE_NATIVE_ARCH` (default: `OFF`): enables `-march=native` for local machine-specific Release tuning.
- `TPS_ENGINE_WARNINGS_AS_ERRORS` (default: `OFF`): treats compiler warnings as errors.

## License
This project is licensed under the GNU General Public License v3.0.
See the [LICENSE](LICENSE) file for the full text.
