#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="build"
TOOLCHAIN_FILE="cmake/toolchains/macos-arm64-homebrew-llvm.cmake"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || printf '8')}"

if ! command -v cmake >/dev/null 2>&1; then
  printf 'cmake not found. Install it with: brew install cmake\n' >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  printf 'ninja not found. Install it with: brew install ninja\n' >&2
  exit 1
fi

if [ ! -x /opt/homebrew/opt/llvm/bin/clang++ ]; then
  printf 'Homebrew LLVM not found at /opt/homebrew/opt/llvm/bin/clang++. Install it with: brew install llvm\n' >&2
  exit 1
fi

if [ "${1:-}" = "--clean" ]; then
  rm -rf "$BUILD_DIR"
elif [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
  if ! grep -q '^CMAKE_CXX_COMPILER:FILEPATH=/opt/homebrew/opt/llvm/bin/clang++$' "$BUILD_DIR/CMakeCache.txt"; then
    printf 'Existing build cache does not use Homebrew LLVM; recreating %s.\n' "$BUILD_DIR"
    rm -rf "$BUILD_DIR"
  fi
fi

cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DVVE_DEFAULT_VULKAN_ICD=kosmickrisp \
  -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple \
  -DVVE_VCPKG_TRIPLET=arm64-osx

cmake --build "$BUILD_DIR" --parallel "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure

printf '\nBuild complete. Executables: bin/debug/exe\n'
printf 'Verification output: bin/debug/verify\n'
