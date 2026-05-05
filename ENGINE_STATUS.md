# Engine Status - Version 2.1 (Dev)

## Specifications
- **Target OS:** Linux (Arch Linux officially supported)
- **Primary Graphics Backend:** Vulkan
- **Runtime Status:** Functional (Vulkan with Swapchain)
- **Memory/Barrier Model:** Render Graph Active with cross-frame tracking

## Features & Working State
- **Project Configuration:** Builds with CMake on Linux; SDL2 required for windowed mode.
- **Engine Loop:** Fixed-step simulation loop active at 60 Hz (Deterministic).
- **Vulkan Presentation:** Swapchain, image acquisition, and presentation active via SDL2.
- **Render Graph:** Dependency validation mapping, hazard checks, automatic barrier synthesis (intra-frame and cross-frame), and Graphviz visualization.
- **Opt-in Optimizations:** Dynamic culling pre-pass, shadow casters gating, forward+/deferred dynamic switching correctly built within the graph.
- **Vulkan Instrumentation:** Hardware validation layer toggle active `TPS_VK_VALIDATION`. GPU timing and profiling natively active in-engine.
- **Control Interface:** Complete terminal-integrated test-bed active (W,A,S,D, SPACE, Q) for engine structural checks. Windowed output enabled when SDL2 is present.
- **Gameplay System:** Structured wave spawning (up to 5 waves) with difficulty scaling, win/lose progression states, and deterministic random seeding (`TPS_RANDOM_SEED`) for reproducible benchmark runs.


## Pipeline Optimization Notes
- **Render Pipeline Strategy:** Strict adherence to pixel-cost reduction. The engine defaults to conditional passes, eliminating full-screen clearing where unnecessary.
- **Hardware Sync Constraints:** GPU timestamp resolution uses non-blocking query reads directly without inducing `VK_QUERY_RESULT_WAIT_BIT` CPU stalls. Any unresolved scope frames are deferred safely.

## Future Horizons
- Expanding multi-frame command tracking (In-Flight Buffering).
- Vulkan dynamic Swapchain instantiating.
- Full mesh optimization pipeline.

*This engine is certified for real-world deterministic benchmarks.*
