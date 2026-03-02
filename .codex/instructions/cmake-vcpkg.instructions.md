---
description: 'C++ project configuration and package management'
applyTo: '**/*.cmake, **/CMakeLists.txt, **/*.cpp, **/*.h, **/*.hpp'
---

# CMake + vcpkg — concise guidance

This file captures short, practical guidance aligned with the CMake roundtable recommendations: prefer target-based CMake, keep the top-level small, use CMakePresets for reproducible configs, and use vcpkg as a toolchain/manifest (do not add_subdirectory vcpkg).

Strategy
- Default (CI-friendly): keep FetchContent-first for the small set of pinned dependencies used during development and CI.
- Deterministic option: use vcpkg manifest mode (preferred) or vendor `vcpkg` as a submodule when you need strict, per-repo reproducibility.

Core rules
- Use targets and usage requirements (INTERFACE/PRIVATE/PUBLIC). Avoid global include/link settings.
- Keep `CMakeLists.txt` minimal: version, project(), include small helpers, add_subdirectory(src).
- Use `CMakePresets.json` to standardize developer and CI configure options.
- Do not `add_subdirectory()` the vcpkg tree. Use `-DCMAKE_TOOLCHAIN_FILE=` to apply vcpkg.
- Do not mix vcpkg and FetchContent for the same dependency in one build.

Developer quick commands

```powershell
cmake --preset default         # FetchContent/default workflow
cmake --preset vcpkg-x64       # uses vcpkg toolchain (manifest or submodule)
cmake --build build --config Release
```

Repo-vendored vcpkg (only if you choose this path)

```powershell
git submodule add https://github.com/microsoft/vcpkg.git third_party/vcpkg
git submodule update --init --recursive
cd third_party/vcpkg
.\bootstrap-vcpkg.bat
cd ..\..
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=.\third_party\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Manifest mode (recommended when reproducibility matters)
- Add `vcpkg.json` at repo root listing dependencies. CI/devs bootstrap vcpkg and run `vcpkg install` (or let CMake trigger manifest installs). Cache `third_party/vcpkg/packages` and `third_party/vcpkg/archives` in CI.

FetchContent helper guidance
- Centralize FetchContent declarations in `cmake/FetchThirdParties.cmake`. Pin tags/commits and expose canonical target names used by the rest of the tree.

CMakePresets.json (suggestion)
- Provide two configure presets: `default` (FetchContent) and `vcpkg-x64` (sets `CMAKE_TOOLCHAIN_FILE` and `VCPKG_TARGET_TRIPLET`). This reduces CLI and CI differences.

When to change strategy
- Switch default to vcpkg manifest mode when you require stronger reproducibility across developer machines and CI. Otherwise, FetchContent-first is acceptable for lightweight infra and faster iteration.

References
- vcpkg manifests: https://github.com/microsoft/vcpkg/blob/master/docs/specifications/manifests.md
