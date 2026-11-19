#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

TOOLCHAIN_FILE="cmake/toolchains/macos-arm64-homebrew-llvm.cmake"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || printf '8')}"
VARIANT="Release"
ENGINE="complex"
CLEAN=0
RUN_TESTS=1

usage() {
  printf 'Usage: %s [debug|release] [--complex|--simple] [--clean] [--no-tests]   (default: complex release)\n' "$0"
}

for arg in "$@"; do
  case "$arg" in
    debug|Debug|DEBUG)
      VARIANT="Debug"
      ;;
    release|Release|RELEASE)
      VARIANT="Release"
      ;;
    --complex|--simple)
      ENGINE="${arg#--}"
      ;;
    --clean)
      CLEAN=1
      ;;
    --no-tests)
      RUN_TESTS=0
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
OUTPUT_DIR="bin/${VARIANT_LOWER}"
if [ "$ENGINE" = complex ]; then
  BUILD_DIR="build/complex-${VARIANT_LOWER}-macos"
  OUTPUT_DIR="bin/complex/${VARIANT_LOWER}"
fi

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

if ! command -v vcpkg >/dev/null 2>&1; then
  printf 'vcpkg not found. Install vcpkg or add it to PATH before building.\n' >&2
  exit 1
fi

vcpkg install --triplet arm64-osx

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
  -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE="$ENGINE" \
  -DVVE_VCPKG_TRIPLET=arm64-osx

cmake --build "$BUILD_DIR" --parallel "$JOBS"
if [ "$RUN_TESTS" -eq 1 ]; then
  # The Ring Racer game runs carry the label racer and take minutes; run them with ctest -L racer.
  ctest --test-dir "$BUILD_DIR" --output-on-failure -LE racer
fi

printf '\n%s %s build complete. Executables: %s/exe\n' "$ENGINE" "$VARIANT" "$OUTPUT_DIR"
printf 'Verification output: %s/verify\n' "$OUTPUT_DIR"
