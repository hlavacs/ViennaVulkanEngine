@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Builds the Vienna Vulkan Engine on Windows using the Ninja generator, which
rem is required for C++23 `import std` (the Visual Studio generator does not
rem support it). Mirrors build_linux.sh.
rem
rem Usage: build_windows.cmd [debug|release] [--clean] [--no-tests]
rem Requires: Vulkan SDK (VULKAN_SDK set), CMake, Visual Studio 2022+ with the
rem           C++ workload (provides cl, CMake, CTest, and Ninja), and vcpkg.
rem From PowerShell: .\build_windows.cmd debug

pushd "%~dp0" || exit /b 1

set "VARIANT=release"
set "CLEAN=0"
set "RUN_TESTS=1"
set "STAGE=Prerequisite checks"

:parse
if "%~1"=="" goto done_parse
if /I "%~1"=="debug" (set "VARIANT=debug") else if /I "%~1"=="release" (set "VARIANT=release") else if /I "%~1"=="--clean" (set "CLEAN=1") else if /I "%~1"=="--no-tests" (set "RUN_TESTS=0") else if /I "%~1"=="-h" (goto usage) else if /I "%~1"=="--help" (goto usage) else (echo Unknown argument: %~1 & goto usage)
shift
goto parse
:done_parse

if /I "%VARIANT%"=="debug" (set "CONFIG=Debug") else (set "CONFIG=Release")
set "BUILD_DIR=build\%VARIANT%-windows"

if not defined VULKAN_SDK if defined VK_SDK_PATH set "VULKAN_SDK=%VK_SDK_PATH%"
if not defined VULKAN_SDK (echo VULKAN_SDK is not set. Install the Vulkan SDK first. & goto fail)
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (echo Vulkan headers not found under "%VULKAN_SDK%". Check VULKAN_SDK. & goto fail)

rem --- Discover the complete toolchain even from an ordinary PowerShell window ---
set "NEED_VS=0"
for %%T in (cl.exe cmake.exe ctest.exe ninja.exe) do (
    where %%T >nul 2>nul
    if errorlevel 1 set "NEED_VS=1"
)
if "%NEED_VS%"=="1" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (echo vswhere.exe not found; run this from a "x64 Native Tools Command Prompt for VS". & goto fail)
    set "VSINSTALL="
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
    if not defined VSINSTALL (echo No Visual Studio with the C++ toolchain was found. & goto fail)
    echo Initializing MSVC environment from "!VSINSTALL!" ...
    call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
    if errorlevel 1 goto fail
    set "PATH=!VSINSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!VSINSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"
)

for %%T in (cl.exe cmake.exe ctest.exe ninja.exe) do (
    where %%T >nul 2>nul
    if errorlevel 1 (echo %%T not found. Install the C++ workload and C++ CMake tools in Visual Studio Installer. & goto fail)
)

rem --- Locate vcpkg and synchronize the whole manifest on every build ---
set "VCPKG_EXE="
if defined VCPKG_ROOT (
    if not exist "%VCPKG_ROOT%\vcpkg.exe" (echo vcpkg.exe not found under VCPKG_ROOT="%VCPKG_ROOT%". & goto fail)
    set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
)
if not defined VCPKG_EXE for /f "delims=" %%i in ('where vcpkg.exe 2^>nul') do if not defined VCPKG_EXE set "VCPKG_EXE=%%i"
if not defined VCPKG_EXE if exist "C:\vcpkg\vcpkg.exe" set "VCPKG_EXE=C:\vcpkg\vcpkg.exe"
if not defined VCPKG_EXE (
    echo vcpkg was not found. Install it and set VCPKG_ROOT to its directory or add it to PATH.
    goto fail
)
for %%i in ("%VCPKG_EXE%") do for %%d in ("%%~dpi.") do set "VCPKG_ROOT=%%~fd"
set "STAGE=Dependency installation"
echo Synchronizing vcpkg manifest dependencies using "%VCPKG_EXE%"...
"%VCPKG_EXE%" install --triplet x64-windows --vcpkg-root "%VCPKG_ROOT%"
if errorlevel 1 goto fail

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (echo Removing %BUILD_DIR% ... & rmdir /s /q "%BUILD_DIR%")
)

rem --- Diagnose the known AMD layer failure and embed only the proven workaround ---
set "VVE_DISABLE_AMD_LAYER=OFF"
set "VULKANINFO="
if exist "%VULKAN_SDK%\Bin\vulkaninfo.exe" set "VULKANINFO=%VULKAN_SDK%\Bin\vulkaninfo.exe"
if exist "%VULKAN_SDK%\Bin\vulkaninfoSDK.exe" set "VULKANINFO=%VULKAN_SDK%\Bin\vulkaninfoSDK.exe"
if defined VULKANINFO (
    if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
    set "SAVED_VK_LAYERS_DISABLE=!VK_LOADER_LAYERS_DISABLE!"
    set "VK_LOADER_LAYERS_DISABLE="
    "!VULKANINFO!" --summary >"%BUILD_DIR%\vulkan-probe.log" 2>&1
    if errorlevel 1 (
        set "VK_LOADER_LAYERS_DISABLE=VK_LAYER_AMD_switchable_graphics"
        "!VULKANINFO!" --summary >"%BUILD_DIR%\vulkan-probe-amd-disabled.log" 2>&1
        if not errorlevel 1 (
            set "VVE_DISABLE_AMD_LAYER=ON"
            echo Detected incompatible AMD switchable-graphics layer. Built programs will disable it for their own process.
        ) else (
            echo Vulkan device discovery failed with and without the AMD layer. See %BUILD_DIR%\vulkan-probe*.log.
        )
    )
    set "VK_LOADER_LAYERS_DISABLE=!SAVED_VK_LAYERS_DISABLE!"
) else (
    echo Vulkan SDK diagnostic tool not found; skipping the runtime compatibility check.
)

rem --- Configure with Ninja (single-config); import-std flags are set by CMakeLists.txt ---
set "STAGE=CMake configuration"
cmake -S . -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DVVE_DEFAULT_VULKAN_ICD=system ^
    -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=simple ^
    -DVVE_VCPKG_TRIPLET=x64-windows ^
    -DVVE_WINDOWS_DISABLE_AMD_SWITCHABLE_GRAPHICS=%VVE_DISABLE_AMD_LAYER%
if errorlevel 1 goto fail

set "STAGE=Compilation and linking"
cmake --build "%BUILD_DIR%"
if errorlevel 1 goto fail

if "%RUN_TESTS%"=="1" (
    set "STAGE=Tests after successful compilation and linking"
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
    if errorlevel 1 goto fail
)

echo.
echo %CONFIG% build complete. Executables: bin\%VARIANT%\exe
popd
exit /b 0

:usage
echo Usage: .\%~nx0 [debug^|release] [--clean] [--no-tests]   (default: release)
popd
exit /b 1

:fail
echo.
echo %STAGE% failed.
popd
exit /b 1
