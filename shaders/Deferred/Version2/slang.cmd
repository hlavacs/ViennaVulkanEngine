@REM This allows passing a path argument to a different slang version
@REM and falls back to default if no argument is passed.
@echo off
set SLANG_PATH=""
if not "%1"=="" set SLANG_PATH=%1

SET FLAGS=-target spirv -g

@echo on
%SLANG_PATH%slangc.exe -version

%SLANG_PATH%slangc.exe 0100_PNUTE.slang %FLAGS% -o 0100_PNUTE.spv
%SLANG_PATH%slangc.exe 1000_PNC.slang %FLAGS% -o 1000_PNC.spv
%SLANG_PATH%slangc.exe 2000_PNO.slang %FLAGS% -o 2000_PNO.spv
%SLANG_PATH%slangc.exe test_lighting.slang %FLAGS% -o test_lighting.spv

pause

@REM ..\..\..\..\..\Downloads\slang_2025-10-3-dev\bin\
