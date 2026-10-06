# AxmolFighter-Editor

A docking editor built on Axmol with Spine 3.4. It edits animation, motion, hitbox (`.box`) data, scenes and layers, and exports NPK files.

## Commands
- Windows: `build.bat` (runs `axmol build -p win32`), `run.bat`.
- Linux: `axmol build -p linux -xc -DAX_WITH_VPX=OFF`. Run it headless on Xvfb `:99`.
- Tests: `-D_AX_TESTS=ON` (doctest, in `Tests/`). Format: `.clang-format`.

## Architecture (service / module / registry)
- Entry points: `Source/AppDelegate.cpp`, `Source/app/EditorRuntime.cpp`, `Source/scene/EditorRootScene.cpp`.
- `core/ServiceRegistry.h` and `core/IService.h` define services. `modules/ModuleHost.cpp` hosts the `*Module` classes.
- `documents/` holds the `*Document` classes and `DocumentRegistry`. `document_editors/` holds the editors for them.
- `asset_browser/` holds the `*Asset` and `*AssetScanner` classes. `editor_properties/` holds the `*EditorProperty` classes.
- Features: `combat/BoxEditor*`, `motion/`, `animation/`, `layer/`, `npk/`, `spine/Spine34Runtime`, `tools/ToolRegistry`.
- New features follow the same pattern: register them through the registries instead of wiring them in directly.
- The `.box` format is shared with `Client/Source/mugen`, so keep the two in sync.
