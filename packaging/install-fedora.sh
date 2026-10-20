#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
if ! command -v cmake >/dev/null || ! command -v ninja >/dev/null; then
  echo 'Install prerequisites first:'
  echo 'sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland'
  exit 1
fi
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build
if command -v update-desktop-database >/dev/null; then update-desktop-database "$HOME/.local/share/applications"; fi
if command -v update-mime-database >/dev/null; then update-mime-database "$HOME/.local/share/mime"; fi
echo 'Flux is installed. Open it from your applications menu or run ~/.local/bin/flux'
