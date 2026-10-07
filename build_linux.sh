#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

CMAKE_BIN="${CMAKE:-$(command -v cmake || true)}"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || printf '8')}"
VARIANT="Release"
CLEAN=0

usage() {
  printf 'Usage: %s [debug|release] [--clean]   (default: release)\n' "$0"
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
BUILD_DIR="build/${VARIANT_LOWER}-linux"
VCPKG_TRIPLET="x64-linux-llvm"
VULKAN_CMAKE_ARGS=()
COMPILER_CMAKE_ARGS=()

CMAKE_BIN="$(command -v "$CMAKE_BIN" || true)"
if [ -z "$CMAKE_BIN" ]; then
  printf 'cmake not found. Set CMAKE or install it with your Linux package manager.\n' >&2
  exit 1
fi

CTEST_BIN="$(dirname "$CMAKE_BIN")/ctest"
if [ ! -x "$CTEST_BIN" ]; then
  CTEST_BIN="$(command -v ctest || true)"
fi
if [ -z "$CTEST_BIN" ]; then
  printf 'ctest not found. Install it with CMake.\n' >&2
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  printf 'ninja not found. Install it with your Linux package manager.\n' >&2
  exit 1
fi

if [ -n "${VULKAN_SDK:-}" ]; then
  VULKAN_CMAKE_ARGS+=("-DVVE_VULKAN_SDK_ROOT=$VULKAN_SDK")
  if [ -f "$VULKAN_SDK/lib/VulkanLoader/lib/libvulkan.so" ]; then
    VULKAN_CMAKE_ARGS+=("-DVulkan_LIBRARY=$VULKAN_SDK/lib/VulkanLoader/lib/libvulkan.so")
  fi
fi

# A matching compiler, scanner and libc++ module are required for import std.
llvm_available() {
  [[ "$1" =~ ^[0-9]+$ ]] &&
    command -v "clang-$1" >/dev/null 2>&1 &&
    command -v "clang++-$1" >/dev/null 2>&1 &&
    command -v "clang-scan-deps-$1" >/dev/null 2>&1 &&
    [ -f "/usr/lib/llvm-$1/lib/libc++.modules.json" ]
}

if [ -z "${VVE_LLVM_VERSION:-}" ]; then
  # Select the newest complete toolchain; incomplete installations are ignored.
  for LLVM_MODULES_JSON in /usr/lib/llvm-*/lib/libc++.modules.json; do
    LLVM_VERSION="${LLVM_MODULES_JSON#/usr/lib/llvm-}"
    LLVM_VERSION="${LLVM_VERSION%%/*}"
    if llvm_available "$LLVM_VERSION" && [ "$LLVM_VERSION" -gt "${VVE_LLVM_VERSION:-0}" ]; then
      VVE_LLVM_VERSION="$LLVM_VERSION"
    fi
  done
fi
if ! llvm_available "${VVE_LLVM_VERSION:-}"; then
  printf 'No suitable LLVM found (VVE_LLVM_VERSION=%s). Need matching clang-NN, clang++-NN, clang-scan-deps-NN and /usr/lib/llvm-NN/lib/libc++.modules.json.\n' "${VVE_LLVM_VERSION:-unset}" >&2
  exit 1
fi
export VVE_LLVM_VERSION
CXX_COMPILER="$(command -v "clang++-$VVE_LLVM_VERSION")"
printf 'Using LLVM %s (%s).\n' "$VVE_LLVM_VERSION" "$CXX_COMPILER"
COMPILER_CMAKE_ARGS+=("-DCMAKE_C_COMPILER=$(command -v "clang-$VVE_LLVM_VERSION")")
COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=$CXX_COMPILER")
COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=$(command -v "clang-scan-deps-$VVE_LLVM_VERSION")")
COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=-stdlib=libc++")
COMPILER_CMAKE_ARGS+=("-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++")
COMPILER_CMAKE_ARGS+=("-DCMAKE_SHARED_LINKER_FLAGS=-stdlib=libc++")
COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_STDLIB_MODULES_JSON=/usr/lib/llvm-$VVE_LLVM_VERSION/lib/libc++.modules.json")

VCPKG_BIN="$(command -v vcpkg || true)"
if [ -n "${VCPKG_ROOT:-}" ] && [ -x "$VCPKG_ROOT/vcpkg" ]; then
  VCPKG_BIN="$VCPKG_ROOT/vcpkg"
fi
if [ -n "$VCPKG_BIN" ]; then
  "$VCPKG_BIN" install --triplet "$VCPKG_TRIPLET"
else
  printf 'vcpkg not found in VCPKG_ROOT or PATH; skipping dependency install.\n'
fi

# Only a compiler change invalidates the build tree automatically.
CACHED_CXX_COMPILER=""
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
  CACHED_CXX_COMPILER="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$BUILD_DIR/CMakeCache.txt")"
fi
if [ "$CLEAN" -eq 1 ]; then
  rm -rf "$BUILD_DIR"
elif [ -n "$CACHED_CXX_COMPILER" ] && [ "$CACHED_CXX_COMPILER" != "$CXX_COMPILER" ]; then
  printf 'Existing build cache uses %s instead of %s; recreating %s.\n' "$CACHED_CXX_COMPILER" "$CXX_COMPILER" "$BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

"$CMAKE_BIN" -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$VARIANT" \
  -DVVE_DEFAULT_VULKAN_ICD=system \
  -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple \
  -DVVE_VCPKG_TRIPLET="$VCPKG_TRIPLET" \
  "${COMPILER_CMAKE_ARGS[@]}" \
  "${VULKAN_CMAKE_ARGS[@]}"

"$CMAKE_BIN" --build "$BUILD_DIR" --parallel "$JOBS"
# Tests that create windows open hidden SDL windows. Use the desktop's video driver when there is a display. On a machine
# without one, fall back to SDL's offscreen driver, which needs a Vulkan driver with VK_EXT_headless_surface.
if [ -z "${SDL_VIDEODRIVER:-}" ] && [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
  export SDL_VIDEODRIVER=offscreen
fi
"$CTEST_BIN" --test-dir "$BUILD_DIR" --output-on-failure

printf '\n%s build complete. Executables: bin/%s/exe\n' "$VARIANT" "$VARIANT_LOWER"
printf 'Verification output: bin/%s/verify\n' "$VARIANT_LOWER"
