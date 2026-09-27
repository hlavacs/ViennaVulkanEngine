@echo off
setlocal EnableExtensions
rem Build Clang modules for ICODA using the installed Windows dependencies.
rem Usage: build_windows_clang.cmd [additional CMake configure arguments]
pushd "%~dp0" || exit /b 1
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio Installer was not found. & exit /b 1)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VVE_VS=%%i"
if not defined VVE_VS (echo Visual Studio C++ tools were not found. & exit /b 1)
call "%VVE_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PATH=%VVE_VS%\VC\Tools\Llvm\x64\bin;%VVE_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VVE_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
for %%T in (clang++.exe clang-scan-deps.exe cmake.exe ninja.exe) do (
    where %%T >nul 2>nul
    if errorlevel 1 (echo %%T was not found. Install the Visual Studio LLVM and CMake tools. & exit /b 1)
)
rem Reuse the downloaded dependency if the MSVC build has already populated it.
set "VVE_LOCAL_DEP="
if exist "build\debug-windows\_deps\viennavulkanpostprocessinglibrary-src\CMakeLists.txt" set "VVE_LOCAL_DEP=-DFETCHCONTENT_SOURCE_DIR_VIENNAVULKANPOSTPROCESSINGLIBRARY=%CD%\build\debug-windows\_deps\viennavulkanpostprocessinglibrary-src"
if defined VVE_LOCAL_DEP (
    cmake --preset debug-clang "%VVE_LOCAL_DEP%" %*
) else (
    cmake --preset debug-clang %*
)
if errorlevel 1 exit /b 1
cmake --build --preset build-debug-clang
if errorlevel 1 exit /b 1
ctest --test-dir build\debug-clang --output-on-failure
exit /b %errorlevel%
