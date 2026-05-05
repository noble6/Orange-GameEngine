# TPS Engine - Next Version Notes (v2.1)

## Goal
Ship a playable, measurable step from prototype systems toward a real-time renderer + demo loop with stronger diagnostics.

## Planned Additions

1. Vulkan Presentation Path
- Add swapchain creation, per-frame image acquisition, and presentation.
- Keep `null` backend fallback for headless/test environments.

2. Render Graph Expansion
- Add cross-frame resource lifetime tracking.
- Add automatic barrier synthesis for transient and persistent resources.
- Emit pass/resource debug visualization output for graph inspection.

3. Frame Pacing & Multi-Buffering
- Introduce in-flight frame resources (double/triple buffered command flow).
- Reduce CPU/GPU sync stalls by decoupling submission and result collection.

4. GPU Metrics Upgrade
- Improve timestamp coverage per pass and add confidence/staleness reporting.
- Add first-pass bandwidth estimation tied to real frame dimensions.

5. Asset & Material Foundation
- Add basic texture loading path with staging/upload flow.
- Add minimal material definition (albedo + roughness/metallic placeholders).

6. Gameplay Demo Upgrade
- Add structured wave spawning and win/lose progression states.
- Add deterministic seed option for reproducible benchmark runs.

7. Testing & Regression Guardrails
- Add smoke test target for playable demo startup.
- Add perf-baseline snapshot output for CI comparisons.

8. Developer UX
- Add runtime debug panel toggles for render passes and budgets.
- Add startup summary log for backend, features, and active quality profile.
