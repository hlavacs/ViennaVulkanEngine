# Toolchain used by VS Code CMake Tools on Apple Silicon macOS.
#
# The engine imports the C++ standard library module (`import std`), which
# currently requires the Homebrew LLVM toolchain plus libc++ module metadata.
# AppleClang from /usr/bin does not provide the required std module setup.

set(CMAKE_SYSTEM_NAME Darwin)

set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS target architecture" FORCE)

set(CMAKE_C_COMPILER "/opt/homebrew/opt/llvm/bin/clang" CACHE FILEPATH "Homebrew LLVM C compiler" FORCE)
set(CMAKE_CXX_COMPILER "/opt/homebrew/opt/llvm/bin/clang++" CACHE FILEPATH "Homebrew LLVM C++ compiler" FORCE)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "/opt/homebrew/opt/llvm/bin/clang-scan-deps" CACHE FILEPATH "Homebrew LLVM clang-scan-deps" FORCE)

set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "Enable C++ standard library module support" FORCE)
set(CMAKE_CXX_STDLIB_MODULES_JSON "/opt/homebrew/opt/llvm/lib/c++/libc++.modules.json" CACHE FILEPATH "libc++ standard library module metadata" FORCE)
