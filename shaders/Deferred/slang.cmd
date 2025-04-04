SET FLAGS=-target spirv -g

slangc.exe test_geometry_vert.slang %FLAGS% -o test_geometry_vert.spv -entry vertexMain
slangc.exe test_geometry_frag.slang %FLAGS% -o test_geometry_frag.spv -entry fragmentMain

slangc.exe test_lighting_vert.slang %FLAGS% -o test_lighting_vert.spv -entry main
slangc.exe test_lighting_frag.slang %FLAGS% -o test_lighting_frag.spv -entry main

pause