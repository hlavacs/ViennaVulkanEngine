SET FLAGS=-target spirv -g

slangc.exe test_vert.slang %FLAGS% -o test_vert.spv -entry vertexMain
slangc.exe test_frag.slang %FLAGS% -o test_frag.spv -entry fragmentMain

pause