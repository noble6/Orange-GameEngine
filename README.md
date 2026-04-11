# Third Person Shooter Engine (TPS Engine)

> **A C++ optimization-first game engine built for performance, determinism, and real-world efficiency.**
> Designed for developers who care about *actual rendering cost*, not just visual tricks.

---

## ⚡ What is this?

TPS Engine is a **performance-driven game engine prototype** written in C++, focused on:

*  Deterministic simulation
*  Measurable rendering cost
*  Low-overhead architecture
*  Efficient GPU/CPU utilization

This is **not** a "throw hardware at it" engine.
This is an engine built on the idea that:

> *Good engineering beats brute force.*

---

## 🧬 Core Philosophy

* Optimize for **real cost per pixel**, not just visuals.
* Avoid unnecessary **full-screen passes**.
* Use **depth, stencil, and visibility** to eliminate wasted work.
* Prefer **compact data formats** over bloated buffers.
* Make performance **measurable, visible, and controllable**.
* Design systems that scale by **efficiency**, not hardware brute force.

---

## 🛠️ Current Features

* 🔁 Deterministic **60 Hz simulation loop**
* 👁️ **Culling-driven rendering pipeline**
* 🔀 Dynamic **Forward+ / Deferred-lite switching**
* 🌑 **Shadow caster budgeting system**
* 📊 Real-time **performance overlay**
* ⏱️ CPU + GPU (Vulkan) **timing instrumentation**
* 🧩 Modular **RHI abstraction** (`null` + `vulkan`)
* 📉 Frame budget tracking with **live diagnostics**

---

## 🧱 Project Structure

```
third-person-shooter-engine/
├── engine/
│   ├── core/         # loop, profiler, math
│   ├── graphics/     # renderer, passes, diagnostics
│   ├── input/        # terminal + fallback input
│   ├── physics/      # fixed-step simulation hooks
│   ├── rhi/          # rendering backend abstraction
│   └── optimization/ # profiling / legacy paths
├── game/
│   ├── player/
│   └── enemies/
├── assets/
├── include/
├── CMakeLists.txt
└── README.md
```

---

## ⚙️ Build Instructions

### 🔧 Release Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### 🧪 Strict Debug Build

```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Debug -DTPS_ENGINE_WARNINGS_AS_ERRORS=ON
cmake --build build-strict --parallel
```

---

## ▶️ Run

```bash
./build/third_person_shooter
```

---

## 🎮 Controls

| Key     | Action |
| ------- | ------ |
| W A S D | Move   |
| SPACE   | Shoot  |
| Q       | Quit   |

---

## 🧠 Rendering Pipeline

```
1. visibility
2. depth_prepass (optional)
3. shadow (budgeted)
4. lighting (forward+ / deferred-lite)
5. volumetric_fog (optional)
6. transparent
7. post (optional)
```

### 💡 Key Design Choices

* **Conditional passes only** — no blind full-screen cost
* **Shadow budgets** instead of unlimited cascades
* **Dynamic pipeline selection** based on scene pressure
* **Minimal overdraw philosophy**

---

## 🎛️ Runtime Configuration

All tuning is controlled via **environment variables**.

### 🎚️ Quality Controls

```bash
TPS_DEPTH_PREPASS=0|1
TPS_SHADOWS=0|1
TPS_FORCE_DEFERRED=0|1
TPS_SSAO=0|1
TPS_FOG=0|1
TPS_POST=0|1
TPS_MSAA=1|2|4|8
```

### ⚖️ Performance Budgets

```bash
TPS_CULL_DISTANCE=<float>
TPS_MAX_VISIBLE_ENEMIES=<int>
TPS_SHADOW_CASTER_BUDGET=<int>
TPS_TARGET_FRAME_MS=<float>
```

### 📊 Diagnostics

```bash
TPS_OVERLAY=0|1
TPS_OVERLAY_EVERY_N_FRAMES=<int>
```

### 🧩 Backend Selection

```bash
TPS_RHI_BACKEND=null|vulkan
```

---

## 📈 Performance Overlay

Real-time metrics include:

* Visible vs culled objects
* Shadow caster selection
* Estimated pixel cost & overdraw
* Per-pass CPU timings
* Estimated bandwidth usage
* Frame budget status (`OK` / `OVER`)
* Active rendering backend

---

## 🔬 Vulkan Backend Notes

* Uses **query-pool timestamps** for GPU profiling
* Currently **headless (no swapchain)** — focused on instrumentation
* Automatically falls back to `null` backend if Vulkan fails
* GPU timings disabled in fallback mode

---

## ⚙️ Example Profiles

### 🟢 Low-Cost Mode

```bash
TPS_SHADOWS=0 TPS_FOG=0 TPS_POST=0 TPS_MAX_VISIBLE_ENEMIES=128 ./build/third_person_shooter
```

### 🔵 High-Quality Mode

```bash
TPS_MSAA=2 TPS_SHADOWS=1 TPS_POST=1 TPS_TARGET_FRAME_MS=16.67 ./build/third_person_shooter
```

---

## 🚧 What's Stubbed (for now)

* Full GPU rendering backend
* Real raster output (currently ASCII debug view)
* Resource barriers & full render graph
* Asset import + compression pipeline
* Final GPU timing resolve

---

## 🗺️ Roadmap

* [ ] Full Vulkan renderer (swapchain + presentation)
* [ ] Accurate GPU bandwidth + timing metrics
* [ ] Texture compression pipeline (BCn + packing)
* [ ] Render graph with dependency tracking
* [ ] Automated performance regression testing
* [ ] Advanced material system
* [ ] Real-time lighting improvements

---

## 🧠 Engine Philosophy (TL;DR)

> **Modern engines waste work. This one refuses to.**

* No blind full-resolution effects
* No oversized buffers
* No hidden costs
* No fake performance via upscaling tricks

Just **clean, measurable, efficient rendering.**

---

## ⚖️ License

Licensed under **GNU GPL v3.0**
See [LICENSE](LICENSE)

---

## 👤 Author

Built by a developer who believes:

> *Optimization is a mindset, not a post-process.*

---

## ⭐ Final Note

This engine is not trying to compete with AAA engines.

It's trying to prove something:

> ⚡ **That efficiency still matters.**
