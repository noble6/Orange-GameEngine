# Third Person Shooter Engine

A C++ optimization-first shooter engine prototype focused on deterministic updates, measurable rendering cost, and practical solo/small-team iteration.

## Core Philosophy
- Optimize for real cost per rendered pixel.
- Prefer compact data paths over brute-force full-screen work.
- Use visibility, depth, and stencil-style thinking to avoid redundant shading.
- Keep expensive techniques conditional and scene-dependent.
- Make every major performance decision measurable.

## Current State
- Playable terminal sample loop with deterministic 60 Hz simulation.
- Culling-driven renderer pass pipeline.
- Forward+/deferred-lite path selection based on scene pressure.
- Shadow caster budget and visibility cap controls.
- CPU pass timings, estimated bandwidth, and frame budget status overlay.
- Explicit RHI abstraction with `null` and Vulkan backends.
- GPU timestamp-scope API wired at pass level with Vulkan query-pool implementation.

## Project Layout
```text
third-person-shooter-engine/
├── engine/
│   ├── core/         # loop, profiler, math
│   ├── graphics/     # renderer, pass scheduling, diagnostics
│   ├── input/        # terminal input + deterministic fallback
│   ├── physics/      # fixed-step physics hooks
│   ├── rhi/          # backend abstraction + stubs
│   └── optimization/ # legacy profiler include path
├── game/
│   ├── player/
│   └── enemies/
├── assets/
├── include/
├── CMakeLists.txt
└── README.md
```

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Strict Build
```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Debug -DTPS_ENGINE_WARNINGS_AS_ERRORS=ON
cmake --build build-strict --parallel
```

## Run
```bash
./build/third_person_shooter
```

## Controls
- `W A S D`: move
- `SPACE`: shoot
- `Q`: quit

## Renderer Pass Order
1. `visibility`
2. `depth_prepass` (optional)
3. `shadow` (optional, budgeted casters)
4. `lighting` (forward+ or deferred-lite)
5. `volumetric_fog` (optional)
6. `transparent`
7. `post` (optional)

## Runtime Tuning Knobs
Set via environment variables.

### Quality / Feature Switches
- `TPS_DEPTH_PREPASS=0|1` (default `1`)
- `TPS_SHADOWS=0|1` (default `1`)
- `TPS_FORCE_DEFERRED=0|1` (default `0`)
- `TPS_SSAO=0|1` (default `0`)
- `TPS_FOG=0|1` (default `0`)
- `TPS_POST=0|1` (default `1`)
- `TPS_MSAA=1|2|4|8` (default `1`)

### Cost Control / Budget Knobs
- `TPS_CULL_DISTANCE=<float>` (default `20.0`)
- `TPS_MAX_VISIBLE_ENEMIES=<int>` (default `256`)
- `TPS_SHADOW_CASTER_BUDGET=<int>` (default `48`)
- `TPS_TARGET_FRAME_MS=<float>` (default `16.67`)

### Overlay / Diagnostics
- `TPS_OVERLAY=0|1` (default `1`)
- `TPS_OVERLAY_EVERY_N_FRAMES=<int>` (default `8`)

### RHI Backend
- `TPS_RHI_BACKEND=null|vulkan` (default `null`, `vulkan_stub` also accepted)

## Vulkan Timestamp Backend Notes
- When compiled with Vulkan SDK available, selecting `TPS_RHI_BACKEND=vulkan` enables a real query-pool timestamp path.
- Current Vulkan path is headless (no swapchain) and focused on pass timing instrumentation.
- If Vulkan initialization fails at runtime (no compatible device/driver/permissions), renderer falls back to `null` backend automatically.
- In fallback mode, GPU timing fields remain unavailable and CPU timings stay active.

## Example Profiles
Lower cost profile:
```bash
TPS_SHADOWS=0 TPS_FOG=0 TPS_POST=0 TPS_MAX_VISIBLE_ENEMIES=128 ./build/third_person_shooter
```

Higher quality profile:
```bash
TPS_MSAA=2 TPS_SHADOWS=1 TPS_POST=1 TPS_TARGET_FRAME_MS=16.67 ./build/third_person_shooter
```

## Overlay Metrics
- Submitted, visible, and culled objects.
- Shadow casters selected after budget/cull.
- Estimated shaded and overdraw pixel load.
- Per-pass CPU ms and estimated bytes touched.
- Frame CPU budget status (`OK` / `OVER`).
- Active RHI backend.

## What Is Intentionally Stubbed
- Real GPU backend execution and resource barriers.
- Actual GPU timestamp query resolution.
- Real rasterization/graphics API output (current visual is ASCII diagnostic view).
- Asset import/compression pipeline implementation.

## Next Recommended Milestones
1. Implement real Vulkan backend with query-pool timestamp resolve.
2. Replace estimated bandwidth counters with measured GPU counters where available.
3. Add compact material/texture pipeline with BCn conversion and packed masks.
4. Add render graph resource lifetime/transitions and pass dependency validation.
5. Add automated perf regression checks in CI from fixed deterministic camera runs.

## CMake Options
- `TPS_ENGINE_ENABLE_IPO` (default `ON`)
- `TPS_ENGINE_ENABLE_NATIVE_ARCH` (default `OFF`)
- `TPS_ENGINE_WARNINGS_AS_ERRORS` (default `OFF`)

## License
GNU General Public License v3.0. See [LICENSE](LICENSE).
