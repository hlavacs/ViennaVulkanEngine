SET FLAGS=-target spirv -g

slangc.exe 0100_PNUTE.slang %FLAGS% -o 0100_PNUTE.spv
slangc.exe test_lighting.slang %FLAGS% -o test_lighting.spv

pause