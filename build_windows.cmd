@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Builds the Vienna Vulkan Engine on Windows using the Ninja generator, which
rem is required for C++23 `import std` (the Visual Studio generator does not
rem support it). Mirrors build_linux.sh.
rem
rem Usage: build_windows.cmd [debug|release] [--clean] [--no-tests]
rem Requires: Vulkan SDK (VULKAN_SDK set), CMake, Visual Studio 2022+ with the
rem           C++ workload (provides cl and Ninja), and vcpkg on PATH for the
rem           first run only.

pushd "%~dp0"

set "VARIANT=release"
set "CLEAN=0"
set "RUN_TESTS=1"

:parse
if "%~1"=="" goto done_parse
if /I "%~1"=="debug" (set "VARIANT=debug") else if /I "%~1"=="release" (set "VARIANT=release") else if /I "%~1"=="--clean" (set "CLEAN=1") else if /I "%~1"=="--no-tests" (set "RUN_TESTS=0") else if /I "%~1"=="-h" (goto usage) else if /I "%~1"=="--help" (goto usage) else (echo Unknown argument: %~1 & goto usage)
shift
goto parse
:done_parse

if /I "%VARIANT%"=="debug" (set "CONFIG=Debug") else (set "CONFIG=Release")
set "BUILD_DIR=build\%VARIANT%-windows"

if not defined VULKAN_SDK (echo VULKAN_SDK is not set. Install the Vulkan SDK first. & goto fail)

rem --- Ensure the MSVC toolchain (cl + Ninja) is on PATH ---
where cl >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (echo vswhere.exe not found; run this from a "x64 Native Tools Command Prompt for VS". & goto fail)
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
    if not defined VSINSTALL (echo No Visual Studio with the C++ toolchain was found. & goto fail)
    echo Initializing MSVC environment from "!VSINSTALL!" ...
    call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
    if errorlevel 1 goto fail
)

where cmake >nul 2>nul
if errorlevel 1 (echo cmake not found on PATH. & goto fail)
where ninja >nul 2>nul
if errorlevel 1 (echo ninja not found. Install the "C++ CMake tools" component in the Visual Studio Installer. & goto fail)

rem --- Bootstrap vcpkg manifest dependencies on first run (sdl3, assimp) ---
if not exist "vcpkg_installed\x64-windows" (
    where vcpkg >nul 2>nul
    if errorlevel 1 (echo vcpkg_installed\x64-windows is missing and vcpkg was not found on PATH. & goto fail)
    echo Installing vcpkg manifest dependencies...
    call vcpkg install
    if errorlevel 1 goto fail
)

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (echo Removing %BUILD_DIR% ... & rmdir /s /q "%BUILD_DIR%")
)

rem --- Configure with Ninja (single-config); import-std flags are set by CMakeLists.txt ---
cmake -S . -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DVVE_DEFAULT_VULKAN_ICD=system ^
    -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple ^
    -DVVE_VCPKG_TRIPLET=x64-windows
if errorlevel 1 goto fail

cmake --build "%BUILD_DIR%"
if errorlevel 1 goto fail

if "%RUN_TESTS%"=="1" (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
    if errorlevel 1 goto fail
)

echo.
echo %CONFIG% build complete. Executables: bin\%VARIANT%\exe
popd
exit /b 0

:usage
echo Usage: %~nx0 [debug^|release] [--clean] [--no-tests]   (default: release)
popd
exit /b 1

:fail
echo.
echo Build failed.
popd
exit /b 1
