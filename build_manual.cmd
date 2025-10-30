@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM Vienna Vulkan Engine - Manual Build Script
REM This script downloads dependencies and builds the project
REM
REM Usage:
REM   build_manual.cmd [build_type] [compiler]
REM
REM Parameters:
REM   build_type: debug or release (optional, will prompt if not specified)
REM   compiler:   msvc or clang (optional, will prompt if not specified)
REM
REM Examples:
REM   build_manual.cmd debug msvc
REM   build_manual.cmd release clang
REM   build_manual.cmd debug
REM   build_manual.cmd
REM ============================================================================

echo.
echo ========================================
echo Vienna Vulkan Engine - Manual Build
echo ========================================
echo.

REM ----------------------------------------------------------------------------
REM Check prerequisites
REM ----------------------------------------------------------------------------

where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Git is not installed or not in PATH
    exit /b 1
)

where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake is not installed or not in PATH
    exit /b 1
)

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK environment variable is not set
    echo Please install the Vulkan SDK from https://vulkan.lunarg.com/
    exit /b 1
)

echo Found Vulkan SDK at: %VULKAN_SDK%
echo.

REM ----------------------------------------------------------------------------
REM Parse command-line arguments for Build Type
REM ----------------------------------------------------------------------------

set BUILD_TYPE_ARG=%~1
set COMPILER_ARG=%~2

if "%BUILD_TYPE_ARG%"=="" (
    goto ASK_BUILD_TYPE
)

REM Normalize to lowercase for comparison
set BUILD_TYPE_ARG_LOWER=%BUILD_TYPE_ARG%
call :StrToLower BUILD_TYPE_ARG_LOWER

if "%BUILD_TYPE_ARG_LOWER%"=="debug" (
    set BUILD_TYPE=Debug
    set BUILD_TYPE_FLAG=/Od /Zi /D_DEBUG
    set RUNTIME_LIB=/MDd
    goto BUILD_TYPE_SET
) else if "%BUILD_TYPE_ARG_LOWER%"=="release" (
    set BUILD_TYPE=Release
    set BUILD_TYPE_FLAG=/O2 /DNDEBUG
    set RUNTIME_LIB=/MD
    goto BUILD_TYPE_SET
) else (
    echo ERROR: Invalid build type "%BUILD_TYPE_ARG%"
    echo Valid options: debug, release
    exit /b 1
)

:ASK_BUILD_TYPE
echo Select Build Type:
echo [1] Debug
echo [2] Release
echo.
set /p BUILD_TYPE_CHOICE="Enter choice (1 or 2): "

if "%BUILD_TYPE_CHOICE%"=="1" (
    set BUILD_TYPE=Debug
    set BUILD_TYPE_FLAG=/Od /Zi /D_DEBUG
    set RUNTIME_LIB=/MDd
) else if "%BUILD_TYPE_CHOICE%"=="2" (
    set BUILD_TYPE=Release
    set BUILD_TYPE_FLAG=/O2 /DNDEBUG
    set RUNTIME_LIB=/MD
) else (
    echo Invalid choice. Please try again.
    goto ASK_BUILD_TYPE
)

:BUILD_TYPE_SET
echo Selected Build Type: %BUILD_TYPE%
echo.

REM ----------------------------------------------------------------------------
REM Parse command-line arguments for Compiler
REM ----------------------------------------------------------------------------

if "%COMPILER_ARG%"=="" (
    goto ASK_TOOLCHAIN
)

REM Normalize to lowercase for comparison
set COMPILER_ARG_LOWER=%COMPILER_ARG%
call :StrToLower COMPILER_ARG_LOWER

if "%COMPILER_ARG_LOWER%"=="msvc" (
    set COMPILER=cl
    set COMPILER_NAME=MSVC
    set CXX_COMPILER=cl
    set C_COMPILER=cl
    set CMAKE_GENERATOR=Visual Studio 17 2022
    set CMAKE_TOOLSET=
    goto COMPILER_SET
) else if "%COMPILER_ARG_LOWER%"=="clang" (
    set COMPILER=clang-cl
    set COMPILER_NAME=Clang-CL
    set CXX_COMPILER=clang-cl
    set C_COMPILER=clang-cl
    set CMAKE_GENERATOR=Visual Studio 17 2022
    set CMAKE_TOOLSET=-T ClangCL
    goto COMPILER_SET
) else (
    echo ERROR: Invalid compiler "%COMPILER_ARG%"
    echo Valid options: msvc, clang
    exit /b 1
)

:ASK_TOOLCHAIN
echo Select Compiler Toolchain:
echo [1] MSVC (Microsoft C++ Compiler)
echo [2] Clang-CL (Clang with MSVC compatibility)
echo.
set /p TOOLCHAIN_CHOICE="Enter choice (1 or 2): "

if "%TOOLCHAIN_CHOICE%"=="1" (
    set COMPILER=cl
    set COMPILER_NAME=MSVC
    set CXX_COMPILER=cl
    set C_COMPILER=cl
    set CMAKE_GENERATOR=Visual Studio 17 2022
    set CMAKE_TOOLSET=
) else if "%TOOLCHAIN_CHOICE%"=="2" (
    set COMPILER=clang-cl
    set COMPILER_NAME=Clang-CL
    set CXX_COMPILER=clang-cl
    set C_COMPILER=clang-cl
    set CMAKE_GENERATOR=Visual Studio 17 2022
    set CMAKE_TOOLSET=-T ClangCL
) else (
    echo Invalid choice. Please try again.
    goto ASK_TOOLCHAIN
)

:COMPILER_SET
echo Selected Compiler: %COMPILER_NAME%
echo.

REM ----------------------------------------------------------------------------
REM Find Visual Studio and setup environment
REM ----------------------------------------------------------------------------

echo Setting up Visual Studio environment...
echo.

REM Try to find vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Could not find vswhere.exe
    echo Please ensure Visual Studio 2019 or later is installed
    exit /b 1
)

REM Get Visual Studio installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VS_PATH=%%i
)

if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo ERROR: Could not find vcvarsall.bat
    exit /b 1
)

REM Setup environment for x64
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to setup Visual Studio environment
    exit /b 1
)

echo Visual Studio environment ready
echo.

REM ----------------------------------------------------------------------------
REM Setup directories
REM ----------------------------------------------------------------------------

set ROOT_DIR=%~dp0
set BUILD_DIR=%ROOT_DIR%build
set DEPS_DIR=%BUILD_DIR%\_deps
set OBJ_DIR=%BUILD_DIR%\obj\%BUILD_TYPE%
set BIN_DIR=%BUILD_DIR%\bin\%BUILD_TYPE%
set LIB_DIR=%BUILD_DIR%\lib\%BUILD_TYPE%

REM Create directories
if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"

echo Build configuration:
echo   Compiler: %COMPILER_NAME%
echo   Build Type: %BUILD_TYPE%
echo   Root: %ROOT_DIR%
echo   Build: %BUILD_DIR%
echo   Deps: %DEPS_DIR%
echo.

REM ----------------------------------------------------------------------------
REM Compiler flags
REM ----------------------------------------------------------------------------

set COMMON_FLAGS=/std:c++20 /EHsc /W3 /nologo /DIMGUI_IMPL_VULKAN_NO_PROTOTYPES /D_CRT_SECURE_NO_WARNINGS
set CXX_FLAGS=%COMMON_FLAGS% %BUILD_TYPE_FLAG% %RUNTIME_LIB% /I"%ROOT_DIR%include"
set C_FLAGS=%COMMON_FLAGS% %BUILD_TYPE_FLAG% %RUNTIME_LIB%

REM Parallel compilation for MSVC
if "%COMPILER_NAME%"=="MSVC" (
    set CXX_FLAGS=%CXX_FLAGS% /MP
)

echo ============================================================================
echo STEP 1: DOWNLOADING DEPENDENCIES
echo ============================================================================
echo.

cd /d "%DEPS_DIR%"

REM ----------------------------------------------------------------------------
REM Download all dependencies
REM ----------------------------------------------------------------------------

echo [1/11] Checking stb_image...
if not exist "stb" (
    echo   Downloading stb_image...
    git clone --depth 1 https://github.com/nothings/stb.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [2/11] Checking L2DFileDialog...
if not exist "L2DFileDialog" (
    echo   Downloading L2DFileDialog...
    git clone --depth 1 https://github.com/Limeoats/L2DFileDialog.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [3/11] Checking ViennaEntityComponentSystem...
if not exist "ViennaEntityComponentSystem" (
    echo   Downloading ViennaEntityComponentSystem...
    git clone --depth 1 https://github.com/hlavacs/ViennaEntityComponentSystem.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [4/11] Checking ViennaStrongType...
if not exist "ViennaStrongType" (
    echo   Downloading ViennaStrongType...
    git clone --depth 1 https://github.com/hlavacs/ViennaStrongType.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [5/11] Checking ViennaTypeListLibrary...
if not exist "ViennaTypeListLibrary" (
    echo   Downloading ViennaTypeListLibrary...
    git clone --depth 1 https://github.com/hlavacs/ViennaTypeListLibrary.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [6/11] Checking SDL3...
if not exist "SDL" (
    echo   Downloading SDL3...
    git clone --depth 1 https://github.com/libsdl-org/SDL.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [7/11] Checking SDL_mixer...
if not exist "SDL_mixer" (
    echo   Downloading SDL_mixer...
    git clone https://github.com/libsdl-org/SDL_mixer.git
    cd SDL_mixer
    git checkout b3a6fa8
    echo   Initializing submodules...
    git submodule update --init --recursive
    cd ..
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
    cd SDL_mixer
    REM Check if submodules are initialized
    if not exist "external\ogg\CMakeLists.txt" (
        echo   Initializing submodules...
        git submodule update --init --recursive
    )
    cd ..
)

echo [8/11] Checking Assimp...
if not exist "assimp" (
    echo   Downloading Assimp...
    git clone --depth 1 https://github.com/assimp/assimp.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [9/11] Checking ImGui...
if not exist "imgui" (
    echo   Downloading ImGui...
    git clone --depth 1 https://github.com/ocornut/imgui.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [10/11] Checking GLM...
if not exist "glm" (
    echo   Downloading GLM...
    git clone --depth 1 https://github.com/g-truc/glm.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo [11/11] Checking vk-bootstrap...
if not exist "vk-bootstrap" (
    echo   Downloading vk-bootstrap...
    git clone --depth 1 https://github.com/charles-lunarg/vk-bootstrap.git
    if %ERRORLEVEL% NEQ 0 exit /b 1
) else (
    echo   Already exists
)

echo.

echo ============================================================================
echo STEP 2: BUILDING DEPENDENCIES WITH CMAKE
echo ============================================================================
echo.

REM ----------------------------------------------------------------------------
REM Build SDL3
REM ----------------------------------------------------------------------------

echo [1/4] Building SDL3...
cd /d "%DEPS_DIR%\SDL"
if not exist "build" mkdir build
cd build
echo   Running CMake configuration...
cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_TOOLSET% -A x64 -DSDL_STATIC=ON -DSDL_SHARED=OFF -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: SDL3 CMake configuration failed
    exit /b 1
)
echo   Compiling SDL3 (this may take a few minutes)...
cmake --build . --config %BUILD_TYPE% -j
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: SDL3 build failed
    exit /b 1
)
REM Rename SDL3-static.lib to SDL3.lib for easier linking
if exist "%DEPS_DIR%\SDL\build\%BUILD_TYPE%\SDL3-static.lib" (
    copy /Y "%DEPS_DIR%\SDL\build\%BUILD_TYPE%\SDL3-static.lib" "%DEPS_DIR%\SDL\build\%BUILD_TYPE%\SDL3.lib" >nul
    echo   Renamed SDL3-static.lib to SDL3.lib
)
echo   SDL3 built successfully
echo.

REM ----------------------------------------------------------------------------
REM Build SDL_mixer
REM ----------------------------------------------------------------------------

echo [2/4] Building SDL_mixer...
cd /d "%DEPS_DIR%\SDL_mixer"
if not exist "build" mkdir build
cd build
echo   Running CMake configuration...
cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_TOOLSET% -A x64 -DBUILD_SHARED_LIBS=OFF -DSDL3_DIR=%DEPS_DIR%\SDL\build -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: SDL_mixer CMake configuration failed
    exit /b 1
)
echo   Compiling SDL_mixer (this may take a few minutes)...
cmake --build . --config %BUILD_TYPE% -j
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: SDL_mixer build failed
    exit /b 1
)
REM Rename SDL3_mixer-static.lib to SDL3_mixer.lib for easier linking
if exist "%DEPS_DIR%\SDL_mixer\build\%BUILD_TYPE%\SDL3_mixer-static.lib" (
    copy /Y "%DEPS_DIR%\SDL_mixer\build\%BUILD_TYPE%\SDL3_mixer-static.lib" "%DEPS_DIR%\SDL_mixer\build\%BUILD_TYPE%\SDL3_mixer.lib" >nul
    echo   Renamed SDL3_mixer-static.lib to SDL3_mixer.lib
)
echo   SDL_mixer built successfully
echo.

REM ----------------------------------------------------------------------------
REM Build Assimp
REM ----------------------------------------------------------------------------

echo [3/4] Building Assimp...
cd /d "%DEPS_DIR%\assimp"
if not exist "build" mkdir build
cd build
echo   Running CMake configuration...
cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_TOOLSET% -A x64 ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DASSIMP_BUILD_ZLIB=ON ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Assimp CMake configuration failed
    exit /b 1
)
echo   Compiling Assimp (this may take several minutes)...
cmake --build . --config %BUILD_TYPE% -j
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Assimp build failed
    exit /b 1
)
echo   Assimp built successfully
echo.

REM ----------------------------------------------------------------------------
REM Build vk-bootstrap
REM ----------------------------------------------------------------------------

echo [4/4] Building vk-bootstrap...
cd /d "%DEPS_DIR%\vk-bootstrap"
if not exist "build" mkdir build
cd build
echo   Running CMake configuration...
cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_TOOLSET% -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: vk-bootstrap CMake configuration failed
    exit /b 1
)
echo   Compiling vk-bootstrap...
cmake --build . --config %BUILD_TYPE% -j
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: vk-bootstrap build failed
    exit /b 1
)
echo   vk-bootstrap built successfully
echo.

echo ============================================================================
echo STEP 3: COMPILING MAIN PROJECT
echo ============================================================================
echo.

cd /d "%ROOT_DIR%"

REM ----------------------------------------------------------------------------
REM Setup include paths
REM ----------------------------------------------------------------------------

set IMGUI_DIR=%DEPS_DIR%\imgui
set INCLUDE_PATHS=^
    /I"%ROOT_DIR%include" ^
    /I"%DEPS_DIR%\stb" ^
    /I"%DEPS_DIR%\L2DFileDialog\L2DFileDialog\src" ^
    /I"%DEPS_DIR%\ViennaEntityComponentSystem\include" ^
    /I"%DEPS_DIR%\ViennaStrongType\include" ^
    /I"%DEPS_DIR%\ViennaTypeListLibrary\include" ^
    /I"%DEPS_DIR%\SDL\include" ^
    /I"%DEPS_DIR%\SDL\include\SDL3" ^
    /I"%DEPS_DIR%\SDL_mixer\include" ^
    /I"%DEPS_DIR%\assimp\include" ^
    /I"%DEPS_DIR%\assimp\build\include" ^
    /I"%IMGUI_DIR%" ^
    /I"%IMGUI_DIR%\backends" ^
    /I"%DEPS_DIR%\glm" ^
    /I"%DEPS_DIR%\vk-bootstrap\src" ^
    /I"%VULKAN_SDK%\Include" ^
    /I"%VULKAN_SDK%\Include\volk" ^
    /I"%VULKAN_SDK%\Include\vma"

REM ----------------------------------------------------------------------------
REM Compile ImGui (with incremental compilation)
REM ----------------------------------------------------------------------------

echo [1/2] Compiling ImGui...
set IMGUI_OBJ_DIR=%OBJ_DIR%\imgui
if not exist "%IMGUI_OBJ_DIR%" mkdir "%IMGUI_OBJ_DIR%"

set IMGUI_FILES=imgui imgui_demo imgui_draw imgui_tables imgui_widgets
set IMGUI_BACKEND_FILES=imgui_impl_sdl3 imgui_impl_vulkan
set IMGUI_FILE_COUNT=0
set IMGUI_TOTAL=7

for %%f in (%IMGUI_FILES%) do (
    set /a IMGUI_FILE_COUNT+=1
    call :CompileIfNewer "%IMGUI_DIR%\%%f.cpp" "%IMGUI_OBJ_DIR%\%%f.obj" "!IMGUI_FILE_COUNT!" %IMGUI_TOTAL% "%%f.cpp"
)

for %%f in (%IMGUI_BACKEND_FILES%) do (
    set /a IMGUI_FILE_COUNT+=1
    call :CompileIfNewer "%IMGUI_DIR%\backends\%%f.cpp" "%IMGUI_OBJ_DIR%\%%f.obj" "!IMGUI_FILE_COUNT!" %IMGUI_TOTAL% "backends\%%f.cpp"
)

echo   ImGui compilation complete
echo.

REM ----------------------------------------------------------------------------
REM Compile ViennaVulkanEngine (with incremental compilation)
REM ----------------------------------------------------------------------------

echo [2/2] Compiling ViennaVulkanEngine...
set VVE_OBJ_DIR=%OBJ_DIR%\vve
if not exist "%VVE_OBJ_DIR%" mkdir "%VVE_OBJ_DIR%"

set VVE_FILE_COUNT=0
set VVE_TOTAL=18

for %%f in (
    VEEngine.cpp
    VEGUI.cpp
    VERenderer.cpp
    VERendererForward.cpp
    VERendererForward11.cpp
    VERendererDeferred.cpp
    VERendererDeferredCommon.cpp
    VERendererDeferred11.cpp
    VERendererDeferred13.cpp
    VERendererShadow11.cpp
    VERendererImgui.cpp
    VERendererVulkan.cpp
    VESceneManager.cpp
    VEAssetManager.cpp
    VESoundManagerSDL3.cpp
    VESystem.cpp
    VEWindow.cpp
    VEWindowSDL.cpp
) do (
    set /a VVE_FILE_COUNT+=1
    call :CompileIfNewerVVE "%ROOT_DIR%src\%%f" "%VVE_OBJ_DIR%\%%~nf.obj" "!VVE_FILE_COUNT!" %VVE_TOTAL% "%%f"
)

echo   ViennaVulkanEngine compilation complete
echo.

REM ----------------------------------------------------------------------------
REM Create static library
REM ----------------------------------------------------------------------------

echo [3/4] Creating ViennaVulkanEngine static library...
lib /nologo /OUT:"%LIB_DIR%\viennavulkanengine.lib" ^
    "%VVE_OBJ_DIR%\*.obj" ^
    "%IMGUI_OBJ_DIR%\*.obj"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to create library
    exit /b 1
)

echo   Library created successfully
echo.

REM ----------------------------------------------------------------------------
REM Compile Examples
REM ----------------------------------------------------------------------------

echo [4/4] Compiling Examples...
echo.

REM Setup library paths for linking
set LINK_LIBS=^
    "%LIB_DIR%\viennavulkanengine.lib" ^
    "%DEPS_DIR%\SDL\build\%BUILD_TYPE%\SDL3.lib" ^
    "%DEPS_DIR%\SDL_mixer\build\%BUILD_TYPE%\SDL3_mixer.lib" ^
    "%DEPS_DIR%\vk-bootstrap\build\%BUILD_TYPE%\vk-bootstrap.lib" ^
    "%VULKAN_SDK%\Lib\vulkan-1.lib" ^
    kernel32.lib ^
    user32.lib ^
    gdi32.lib ^
    shell32.lib ^
    ole32.lib ^
    oleaut32.lib ^
    advapi32.lib ^
    winmm.lib ^
    imm32.lib ^
    version.lib ^
    setupapi.lib

REM Find assimp library (name varies by version)
set ASSIMP_LIB=
for %%f in ("%DEPS_DIR%\assimp\build\lib\%BUILD_TYPE%\assimp-*.lib") do (
    set ASSIMP_LIB=%%f
)

REM Find zlib library (built with assimp) - check multiple possible names
set ZLIB_LIB=
for %%f in ("%DEPS_DIR%\assimp\build\contrib\zlib\%BUILD_TYPE%\zlibstaticd.lib") do (
    if exist "%%f" set ZLIB_LIB=%%f
)
if not defined ZLIB_LIB (
    for %%f in ("%DEPS_DIR%\assimp\build\contrib\zlib\%BUILD_TYPE%\zlibstatic.lib") do (
        if exist "%%f" set ZLIB_LIB=%%f
    )
)
if not defined ZLIB_LIB (
    for %%f in ("%DEPS_DIR%\assimp\build\contrib\zlib\%BUILD_TYPE%\zlibd.lib") do (
        if exist "%%f" set ZLIB_LIB=%%f
    )
)
if not defined ZLIB_LIB (
    for %%f in ("%DEPS_DIR%\assimp\build\contrib\zlib\%BUILD_TYPE%\zlib.lib") do (
        if exist "%%f" set ZLIB_LIB=%%f
    )
)

echo Debug: ASSIMP_LIB=%ASSIMP_LIB%
echo Debug: ZLIB_LIB=%ZLIB_LIB%

set EXAMPLE_COUNT=0
set EXAMPLE_TOTAL=4

REM Compile game example
set /a EXAMPLE_COUNT+=1
call :CompileExample "game" "%ROOT_DIR%examples\game\game.cpp" "!EXAMPLE_COUNT!" %EXAMPLE_TOTAL%

REM Compile helper example
set /a EXAMPLE_COUNT+=1
call :CompileExample "helper" "%ROOT_DIR%examples\helper\helper.cpp" "!EXAMPLE_COUNT!" %EXAMPLE_TOTAL%

REM Compile physics example
set /a EXAMPLE_COUNT+=1
call :CompileExample "physics" "%ROOT_DIR%examples\physics\physics.cpp" "!EXAMPLE_COUNT!" %EXAMPLE_TOTAL%

REM Compile deferred-demo example
set /a EXAMPLE_COUNT+=1
call :CompileExample "deferred-demo" "%ROOT_DIR%examples\deferred-demo\deferred-demo.cpp" "!EXAMPLE_COUNT!" %EXAMPLE_TOTAL%

echo.
echo   All examples compiled successfully
echo.

echo ============================================================================
echo BUILD COMPLETE
echo ============================================================================
echo.
echo Build artifacts:
echo   Library: %LIB_DIR%\viennavulkanengine.lib
echo.
echo Examples:
echo   - %BIN_DIR%\game.exe
echo   - %BIN_DIR%\helper.exe
echo   - %BIN_DIR%\physics.exe
echo   - %BIN_DIR%\deferred-demo.exe
echo.
echo Dependencies built with %COMPILER_NAME%:
echo   - %DEPS_DIR%\SDL\build\%BUILD_TYPE%\SDL3.lib
echo   - %DEPS_DIR%\SDL_mixer\build\%BUILD_TYPE%\SDL3_mixer.lib
echo   - %DEPS_DIR%\assimp\build\lib\%BUILD_TYPE%\assimp-*.lib
echo   - %DEPS_DIR%\vk-bootstrap\build\%BUILD_TYPE%\vk-bootstrap.lib
echo.
echo ============================================================================

endlocal
exit /b 0

REM ============================================================================
REM Helper Functions
REM ============================================================================

REM ----------------------------------------------------------------------------
REM Function to compile ImGui file only if source is newer than object
REM Parameters: %1=source file, %2=object file, %3=current count, %4=total, %5=display name
REM ----------------------------------------------------------------------------
:CompileIfNewer
setlocal
set SOURCE_FILE=%~1
set OBJECT_FILE=%~2
set CURRENT=%~3
set TOTAL=%~4
set DISPLAY_NAME=%~5

REM Check if object file exists and is newer than source
if exist "%OBJECT_FILE%" (
    for %%S in ("%SOURCE_FILE%") do set SOURCE_TIME=%%~tS
    for %%O in ("%OBJECT_FILE%") do set OBJECT_TIME=%%~tO

    REM Simple comparison - if object exists, check if we need recompile
    if "%SOURCE_TIME%" LEQ "%OBJECT_TIME%" (
        echo   [%CURRENT%/%TOTAL%] %DISPLAY_NAME% - up to date, skipping
        goto :CompileIfNewerEnd
    )
)

echo   [%CURRENT%/%TOTAL%] Compiling %DISPLAY_NAME%...
%CXX_COMPILER% %CXX_FLAGS% %INCLUDE_PATHS% ^
    /c "%SOURCE_FILE%" ^
    /Fo"%OBJECT_FILE%"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile %DISPLAY_NAME%
    exit /b 1
)

:CompileIfNewerEnd
endlocal
goto :eof

REM ----------------------------------------------------------------------------
REM Function to compile VVE file only if source is newer than object
REM Parameters: %1=source file, %2=object file, %3=current count, %4=total, %5=display name
REM ----------------------------------------------------------------------------
:CompileIfNewerVVE
setlocal
set SOURCE_FILE=%~1
set OBJECT_FILE=%~2
set CURRENT=%~3
set TOTAL=%~4
set DISPLAY_NAME=%~5

REM Check if object file exists and is newer than source
if exist "%OBJECT_FILE%" (
    for %%S in ("%SOURCE_FILE%") do set SOURCE_TIME=%%~tS
    for %%O in ("%OBJECT_FILE%") do set OBJECT_TIME=%%~tO

    REM Simple comparison - if object exists, check if we need recompile
    if "%SOURCE_TIME%" LEQ "%OBJECT_TIME%" (
        echo   [%CURRENT%/%TOTAL%] %DISPLAY_NAME% - up to date, skipping
        goto :CompileIfNewerVVEEnd
    )
)

echo   [%CURRENT%/%TOTAL%] Compiling %DISPLAY_NAME%...
%CXX_COMPILER% %CXX_FLAGS% %INCLUDE_PATHS% ^
    /c "%SOURCE_FILE%" ^
    /Fo"%OBJECT_FILE%"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile %DISPLAY_NAME%
    exit /b 1
)

:CompileIfNewerVVEEnd
endlocal
goto :eof

REM ----------------------------------------------------------------------------
REM Function to compile and link an example executable
REM Parameters: %1=executable name, %2=source file, %3=current count, %4=total
REM ----------------------------------------------------------------------------
:CompileExample
setlocal EnableDelayedExpansion
set EXAMPLE_NAME=%~1
set SOURCE_FILE=%~2
set CURRENT=%~3
set TOTAL=%~4
set EXE_FILE=%BIN_DIR%\%EXAMPLE_NAME%.exe
set OBJ_FILE=%OBJ_DIR%\examples\%EXAMPLE_NAME%.obj

REM Create obj directory for examples
if not exist "%OBJ_DIR%\examples" mkdir "%OBJ_DIR%\examples"

echo   [%CURRENT%/%TOTAL%] Compiling %EXAMPLE_NAME%...

REM Check if we need to recompile
set NEED_COMPILE=1
if exist "%OBJ_FILE%" (
    if exist "%EXE_FILE%" (
        for %%S in ("%SOURCE_FILE%") do set SOURCE_TIME=%%~tS
        for %%O in ("%OBJ_FILE%") do set OBJECT_TIME=%%~tO
        for %%E in ("%EXE_FILE%") do set EXE_TIME=%%~tE

        REM If source hasn't changed and exe exists, skip
        if "!SOURCE_TIME!" LEQ "!OBJECT_TIME!" (
            if "!OBJECT_TIME!" LEQ "!EXE_TIME!" (
                echo      %EXAMPLE_NAME%.exe is up to date, skipping
                set NEED_COMPILE=0
            )
        )
    )
)

if !NEED_COMPILE!==1 (
    REM Compile the source file
    echo      Compiling %EXAMPLE_NAME%.cpp...
    echo.
    echo      === COMPILE COMMAND ===
    echo      %CXX_COMPILER% %CXX_FLAGS% %INCLUDE_PATHS% /c "%SOURCE_FILE%" /Fo"%OBJ_FILE%"
    echo      =======================
    echo.
    %CXX_COMPILER% %CXX_FLAGS% %INCLUDE_PATHS% ^
        /c "%SOURCE_FILE%" ^
        /Fo"%OBJ_FILE%"

    if !ERRORLEVEL! NEQ 0 (
        echo ERROR: Failed to compile %EXAMPLE_NAME%
        exit /b 1
    )

    REM Link the executable
    echo.
    echo      Linking %EXAMPLE_NAME%.exe...
    echo.
    echo      === LINK COMMAND ===
    if defined ZLIB_LIB (
        echo      link /nologo /OUT:"%EXE_FILE%" "%OBJ_FILE%" %LINK_LIBS% "%ASSIMP_LIB%" "%ZLIB_LIB%" /SUBSYSTEM:CONSOLE
    ) else (
        echo      link /nologo /OUT:"%EXE_FILE%" "%OBJ_FILE%" %LINK_LIBS% "%ASSIMP_LIB%" /SUBSYSTEM:CONSOLE
    )
    echo      ====================
    echo.

    if defined ZLIB_LIB (
        link /nologo /OUT:"%EXE_FILE%" ^
            "%OBJ_FILE%" ^
            %LINK_LIBS% ^
            "%ASSIMP_LIB%" ^
            "%ZLIB_LIB%" ^
            /SUBSYSTEM:CONSOLE
    ) else (
        link /nologo /OUT:"%EXE_FILE%" ^
            "%OBJ_FILE%" ^
            %LINK_LIBS% ^
            "%ASSIMP_LIB%" ^
            /SUBSYSTEM:CONSOLE
    )

    if !ERRORLEVEL! NEQ 0 (
        echo ERROR: Failed to link %EXAMPLE_NAME%
        exit /b 1
    )

    echo      %EXAMPLE_NAME%.exe created successfully
)

endlocal
goto :eof

REM ----------------------------------------------------------------------------
REM Helper function to convert string to lowercase
REM ----------------------------------------------------------------------------
:StrToLower
setlocal EnableDelayedExpansion
set "str=!%~1!"
set "str=%str:A=a%"
set "str=%str:B=b%"
set "str=%str:C=c%"
set "str=%str:D=d%"
set "str=%str:E=e%"
set "str=%str:F=f%"
set "str=%str:G=g%"
set "str=%str:H=h%"
set "str=%str:I=i%"
set "str=%str:J=j%"
set "str=%str:K=k%"
set "str=%str:L=l%"
set "str=%str:M=m%"
set "str=%str:N=n%"
set "str=%str:O=o%"
set "str=%str:P=p%"
set "str=%str:Q=q%"
set "str=%str:R=r%"
set "str=%str:S=s%"
set "str=%str:T=t%"
set "str=%str:U=u%"
set "str=%str:V=v%"
set "str=%str:W=w%"
set "str=%str:X=x%"
set "str=%str:Y=y%"
set "str=%str:Z=z%"
endlocal & set "%~1=%str%"
goto :eof