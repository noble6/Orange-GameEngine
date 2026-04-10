# Next Session TODO (Unfinished Work)

Last updated: 2026-04-10

## What is already done
- Playable terminal prototype (`WASD`, `SPACE`, `Q`).
- Optimization-focused pass pipeline (visibility, depth prepass, shadow budget, lighting path select, transparent, post).
- Runtime perf knobs and frame diagnostics overlay.
- RHI abstraction with `null` and Vulkan-backed timestamp path + fallback.
- README upgraded with architecture, knobs, and roadmap.

## Highest priority unfinished items
1. Vulkan backend robustness
- Add explicit init-failure reason logging (instance/device/queue/query pool/fence).
- Add optional validation layers in debug builds.
- Add non-blocking timestamp resolve path (avoid hard waits where possible).

2. Real render architecture
- Implement real render graph (resource lifetime + pass dependencies + barrier validation).
- Replace estimated bandwidth counters with actual GPU counters where available.
- Add command generation split for better CPU/GPU parallelism.

3. Asset pipeline
- Texture import pipeline with BCn compression and automatic channel packing.
- Mesh optimization + LOD build steps.
- Material/shader build pipeline and permutation controls.
- Runtime streaming/budget manager.

4. Rendering feature expansion (cost-controlled)
- Decal system with tight projection/culling.
- Particle system with overdraw controls.
- Water/wetness path with minimal extra passes.
- Volumetric fog in real GPU path with strict resolution budget.

5. Engine systems
- Job system + task graph for culling/asset prep/command prep.
- Lightweight ECS/scene data model for cache-friendly updates.
- Memory tracking and allocator strategy for hot paths.

## Quality/perf gates to add
- Per-scene perf budget checks in CI.
- Regression tests for visible count caps, shadow budget caps, and frame budget status.
- Debug views for culling, overdraw proxy, lighting path decision, and shadow atlas usage.

## Resume checklist (next time)
1. Start with Vulkan init-failure diagnostics and validation-layer switch.
2. Then implement render graph skeleton before adding more effects.
3. Then start asset pipeline (BCn + packing) before expanding materials.
