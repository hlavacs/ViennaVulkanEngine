#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

CMAKE_BIN="/home/hlavacs/vcpkg/downloads/tools/cmake-4.3.3-linux/cmake-4.3.3-linux-x86_64/bin/cmake"
CTEST_BIN="/home/hlavacs/vcpkg/downloads/tools/cmake-4.3.3-linux/cmake-4.3.3-linux-x86_64/bin/ctest"
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

if [ ! -x "$CMAKE_BIN" ]; then
  CMAKE_BIN="$(command -v cmake || true)"
fi
if [ -z "$CMAKE_BIN" ]; then
  printf 'cmake not found. Install it with your Linux package manager.\n' >&2
  exit 1
fi

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

if [ -x /usr/bin/clang++-18 ] && [ -x /usr/bin/clang-scan-deps-18 ]; then
  COMPILER_CMAKE_ARGS+=("-DCMAKE_C_COMPILER=/usr/bin/clang-18")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=/usr/bin/clang++-18")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=/usr/bin/clang-scan-deps-18")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=-stdlib=libc++")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_SHARED_LINKER_FLAGS=-stdlib=libc++")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_STDLIB_MODULES_JSON=/usr/lib/llvm-18/lib/libc++.modules.json")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_CXX_MODULE_STD=ON")
  COMPILER_CMAKE_ARGS+=("-DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
fi
# A triplet change invalidates package paths retained by CMake.
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
  if ! grep -q "^VVE_VCPKG_TRIPLET:.*=$VCPKG_TRIPLET$" "$BUILD_DIR/CMakeCache.txt"; then
    CLEAN=1
  fi
fi

if [ "$CLEAN" -eq 1 ]; then
  rm -rf "$BUILD_DIR"
elif [ -f "$BUILD_DIR/CMakeCache.txt" ] && ! grep -q "^CMAKE_CXX_COMPILER:.*clang++-18$" "$BUILD_DIR/CMakeCache.txt"; then
  printf 'Existing build cache does not use LLVM 18; recreating %s.\n' "$BUILD_DIR"
  rm -rf "$BUILD_DIR"
elif [ -f "$BUILD_DIR/CMakeCache.txt" ] && grep -q '^Vulkan_LIBRARY:FILEPATH=Vulkan_LIBRARY-NOTFOUND$' "$BUILD_DIR/CMakeCache.txt"; then
  printf 'Existing build cache did not find the Vulkan loader; recreating %s.\n' "$BUILD_DIR"
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
if [ -z "${SDL_VIDEODRIVER:-}" ]; then
  SDL_VIDEODRIVER=offscreen
fi
export SDL_VIDEODRIVER
"$CTEST_BIN" --test-dir "$BUILD_DIR" --output-on-failure

printf '\n%s build complete. Executables: bin/%s/exe\n' "$VARIANT" "$VARIANT_LOWER"
printf 'Verification output: bin/%s/verify\n' "$VARIANT_LOWER"
