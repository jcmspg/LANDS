# AGENTS.md

## Cursor Cloud specific instructions

This repo is a single C++20 rules-engine library (`engine/`) built with CMake + GoogleTest, plus a
command-line simulation harness. There is no long-running service. "Running the app" means building and
running the GoogleTest suites, or running `./engine/build/lands_sim --matches 1000` (plays random-bot
matches and checks termination and bit-for-bit replay). Standard build/test commands live in `README.md`
("Build & test") and `engine/CMakeLists.txt`; use those rather than duplicating them here. Project rules
for contributors (code style, test etiquette, commit rules, current status) live in `CLAUDE.md` and
`docs/`: read `docs/CONVENTIONS.md` before changing code and keep `docs/ENGINE_STATUS.md` current.

### Build / test / run

- Configure + build + test (from repo root):
  - `cmake -S engine -B engine/build`
  - `cmake --build engine/build -j`
  - `ctest --test-dir engine/build --output-on-failure`
- Per-suite binaries are emitted under `engine/build/` (e.g. `test_Deck`, `test_LandGameMatch`).
- Optional ASan + UBSan build: `cmake -S engine -B engine/build-san -DLANDS_SANITIZE=ON`, then build and run `ctest` there.

### Non-obvious gotchas

- The default C++ compiler `/usr/bin/c++` is **Clang** (via `/etc/alternatives/c++`), not GCC, and CMake
  picks it up. Clang auto-selects the newest installed GCC toolchain for its libstdc++, so the matching
  `libstdc++-<N>-dev` must be present or configuration fails with `ld: cannot find -lstdc++`. The startup
  update script installs `libstdc++-14-dev` to cover this. If you switch compilers, ensure the stdlib for
  that GCC version is installed.
- `find_package(GTest REQUIRED)` needs the system GoogleTest CMake config from `libgtest-dev` (installed
  by the update script). GTest is not vendored in-repo.
- There is **no configured linter/formatter** (no `.clang-tidy`/`.clang-format`). For a quick static
  sanity check, compile the sources with warnings enabled:
  `c++ -std=c++20 -Wall -Wextra -fsyntax-only -I engine/include engine/src/*.cpp`.
- Later phases (server, Godot client, auth, deploy) described in `README.md`/`GameDesignDocument.md` are
  **not implemented yet** — only the `engine/` library and its simulation harness exist. A Godot demo is the
  planned next step (`docs/ENGINE_STATUS.md`, roadmap step 15).
