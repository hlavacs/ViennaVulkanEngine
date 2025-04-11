SET FLAGS=-target spirv -g

slangc.exe test_geometry.slang %FLAGS% -o test_geometry.spv
slangc.exe test_lighting.slang %FLAGS% -o test_lighting.spv

pause