SET FLAGS=-target spirv -g

slangc.exe 0100_PNUTE.slang %FLAGS% -o 0100_PNUTE.spv
slangc.exe 1000_PNC.slang %FLAGS% -o 1000_PNC.spv
slangc.exe 2000_PNO.slang %FLAGS% -o 2000_PNO.spv
slangc.exe test_lighting.slang %FLAGS% -o test_lighting.spv

pause
