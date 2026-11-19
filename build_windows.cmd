@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Builds the Vienna Vulkan Engine on Windows using the Ninja generator, which
rem is required for C++23 `import std` (the Visual Studio generator does not
rem support it). Mirrors build_linux.sh.
rem
rem Usage: build_windows.cmd [debug|release] [--complex|--simple|--all] [--clean] [--no-tests] [--docs]
rem Requires: Vulkan SDK (VULKAN_SDK set), CMake, Visual Studio 2022+ with the
rem           C++ workload (provides cl, CMake, CTest, and Ninja), and vcpkg.
rem From PowerShell: .\build_windows.cmd debug

pushd "%~dp0" || exit /b 1

set "VARIANT=release"
set "ENGINE=complex"
set "CLEAN=0"
set "RUN_TESTS=1"
set "BUILD_DOCS=0"
set "STAGE=Prerequisite checks"

:parse
if "%~1"=="" goto done_parse
if /I "%~1"=="debug" (set "VARIANT=debug") else if /I "%~1"=="release" (set "VARIANT=release") else if /I "%~1"=="--all" (set "ENGINE=all") else if /I "%~1"=="--simple" (set "ENGINE=simple") else if /I "%~1"=="--complex" (set "ENGINE=complex") else if /I "%~1"=="--clean" (set "CLEAN=1") else if /I "%~1"=="--no-tests" (set "RUN_TESTS=0") else if /I "%~1"=="--docs" (set "BUILD_DOCS=1") else if /I "%~1"=="-h" (goto usage) else if /I "%~1"=="--help" (goto usage) else (echo Unknown argument: %~1 & goto usage)
shift
goto parse
:done_parse

if /I "%VARIANT%"=="debug" (set "CONFIG=Debug") else (set "CONFIG=Release")

if not defined VULKAN_SDK if defined VK_SDK_PATH set "VULKAN_SDK=%VK_SDK_PATH%"
if not defined VULKAN_SDK (echo VULKAN_SDK is not set. Install the Vulkan SDK first. & goto fail)
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (echo Vulkan headers not found under "%VULKAN_SDK%". Check VULKAN_SDK. & goto fail)

rem --- Always refresh MSVC and prefer Visual Studio CMake over other PATH entries ---
rem VsDevCmd uses dir /b to enumerate its toolchain scripts; inherited options such as /s break that list.
set "DIRCMD="
set "REQUESTED_VCPKG_ROOT=%VCPKG_ROOT%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere.exe not found. Install Visual Studio with the C++ workload. & goto fail)
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo No Visual Studio with the C++ toolchain was found. & goto fail)
rem Explicitly select this installation's default, even in a pre-update developer prompt.
set "MSVC_SETUP=%VSINSTALL%\VC\Auxiliary\Build"
set "MSVC_VERSION="
if exist "%MSVC_SETUP%\Microsoft.VCToolsVersion.default.txt" set /p "MSVC_VERSION="<"%MSVC_SETUP%\Microsoft.VCToolsVersion.default.txt"
if not defined MSVC_VERSION (echo Cannot read the default MSVC version under "%MSVC_SETUP%". Repair the x64 C++ tools in Visual Studio Installer. & goto fail)
set "MSVC_CL=%VSINSTALL%\VC\Tools\MSVC\%MSVC_VERSION%\bin\Hostx64\x64\cl.exe"
if not exist "%MSVC_CL%" (echo Default MSVC compiler not found at "%MSVC_CL%". Repair the x64 C++ tools in Visual Studio Installer. & goto fail)
echo Initializing MSVC %MSVC_VERSION% from "%VSINSTALL%" ...
call "%MSVC_SETUP%\vcvars64.bat" -vcvars_ver=%MSVC_VERSION% >nul
if errorlevel 1 goto refresh_msvc
if defined VCToolsInstallDir if "%VCToolsVersion%"=="%MSVC_VERSION%" if exist "%VCToolsInstallDir%\bin\Hostx64\x64\cl.exe" goto msvc_ready

:refresh_msvc
rem A Developer Command Prompt opened before an update can retain a removed toolset.
call "%MSVC_SETUP%\vcvars64.bat" /clean_env >nul
call "%MSVC_SETUP%\vcvars64.bat" -vcvars_ver=%MSVC_VERSION%
if errorlevel 1 goto fail

:msvc_ready
if not "%VCToolsVersion%"=="%MSVC_VERSION%" (echo Visual Studio could not activate MSVC %MSVC_VERSION%. & goto fail)
if not defined VCToolsInstallDir (echo Visual Studio did not report a C++ toolset directory. & goto fail)
if not exist "%VCToolsInstallDir%\bin\Hostx64\x64\cl.exe" (echo cl.exe not found in "%VCToolsInstallDir%". Repair the x64 C++ tools in Visual Studio Installer. & goto fail)
set "PATH=%VCToolsInstallDir%\bin\Hostx64\x64;%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
if defined REQUESTED_VCPKG_ROOT set "VCPKG_ROOT=%REQUESTED_VCPKG_ROOT%"

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

set "ENGINES=%ENGINE%"
if "%ENGINE%"=="all" set "ENGINES=complex simple"
for %%E in (%ENGINES%) do (
    call :build_engine %%E
    if errorlevel 1 goto fail
)
popd
exit /b 0

:build_engine
set "ENGINE=%~1"
echo Building %ENGINE% %CONFIG%...
set "BUILD_DIR=build\%VARIANT%-windows"
set "OUTPUT_DIR=bin\%ENGINE%\%VARIANT%"
if "%ENGINE%"=="complex" (
    set "BUILD_DIR=build\complex-%VARIANT%-windows"
)

if "%CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (echo Removing %BUILD_DIR% ... & rmdir /s /q "%BUILD_DIR%")
)

rem --- Configure with Ninja (single-config); import-std flags are set by CMakeLists.txt ---
set "STAGE=CMake configuration"
set "FRESH="
for %%C in ("%MSVC_CL%") do set "SELECTED_CL=%%~fsC"
if exist "%BUILD_DIR%\CMakeCache.txt" (
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_CXX_COMPILER:FILEPATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
        for %%C in ("%%B") do if /i not "%%~fsC"=="%SELECTED_CL%" set "FRESH=--fresh"
    )
    for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"CMAKE_LINKER:FILEPATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
        if not exist "%%B" set "FRESH=--fresh"
    )
)
if defined FRESH echo Refreshing the CMake cache for the selected MSVC toolset...
cmake %FRESH% -S . -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DVVE_DEFAULT_VULKAN_ICD=system ^
    -DVVE_ENGINE_IMPLEMENTATION_NAMESPACE=%ENGINE% ^
    -DVVE_VCPKG_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

set "STAGE=Compilation and linking"
cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

if "%BUILD_DOCS%"=="1" (
    set "STAGE=Doxygen documentation"
    cmake --build "%BUILD_DIR%" --target docs
    if errorlevel 1 exit /b 1
)

if "%RUN_TESTS%"=="1" (
    set "STAGE=Tests after successful compilation and linking"
    rem The Ring Racer game runs carry the label racer and take minutes; run them with ctest -L racer.
    ctest --test-dir "%BUILD_DIR%" --output-on-failure -LE racer
    if errorlevel 1 exit /b 1
)

echo.
echo %ENGINE% %CONFIG% build complete. Executables: %OUTPUT_DIR%\exe
exit /b 0

:usage
echo Usage: .\%~nx0 [debug^|release] [--complex^|--simple^|--all] [--clean] [--no-tests] [--docs]   [default: complex release]
popd
exit /b 1

:fail
echo.
echo %STAGE% failed.
popd
exit /b 1
