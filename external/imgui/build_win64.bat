@REM Build for Visual Studio compiler. Run your copy of amd64/vcvars32.bat to setup 64-bit command-line compiler.

@set INCLUDES=/I..\glfw\include /I %VULKAN_SDK%\include
@set SOURCES=main.cpp imgui*.cpp
@set LIBS=/LIBPATH:..\glfw\lib-vc2010-64 /libpath:%VULKAN_SDK%\lib glfw3.lib opengl32.lib gdi32.lib shell32.lib vulkan-1.lib

@set OUT_EXE=example

@set OUT_DIR=Debug
mkdir %OUT_DIR%
cl /nologo /Zi /MD %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%

@set OUT_DIR=Release
mkdir %OUT_DIR%
cl /nologo /Zi /MD /Ox /Oi %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
