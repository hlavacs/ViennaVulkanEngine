#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

TOOLCHAIN_FILE="cmake/toolchains/macos-arm64-homebrew-llvm.cmake"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || printf '8')}"
VARIANT="Debug"
CLEAN=0

usage() {
  printf 'Usage: %s [debug|release] [--clean]\n' "$0"
  printf '       %s --clean [debug|release]\n' "$0"
}

for arg in "$@"; do
  case "$arg" in
    debug|Debug|DEBUG)
      VARIANT="Debug"
      ;;
    release|Release|RELEASE)
      VARIANT="Release"
      ;;
    --clean)
      CLEAN=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 1
      ;;
  esac
done

VARIANT_LOWER="$(printf '%s' "$VARIANT" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="build/macos-${VARIANT_LOWER}"

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

if [ "$CLEAN" -eq 1 ]; then
  rm -rf "$BUILD_DIR"
elif [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
  if ! grep -q '^CMAKE_CXX_COMPILER:FILEPATH=/opt/homebrew/opt/llvm/bin/clang++$' "$BUILD_DIR/CMakeCache.txt"; then
    printf 'Existing build cache does not use Homebrew LLVM; recreating %s.\n' "$BUILD_DIR"
    rm -rf "$BUILD_DIR"
  fi
fi

cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$VARIANT" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DVVE_DEFAULT_VULKAN_ICD=kosmickrisp \
  -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple \
  -DVVE_VCPKG_TRIPLET=arm64-osx

cmake --build "$BUILD_DIR" --parallel "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure

printf '\n%s build complete. Executables: bin/%s/exe\n' "$VARIANT" "$VARIANT_LOWER"
printf 'Verification output: bin/%s/verify\n' "$VARIANT_LOWER"
