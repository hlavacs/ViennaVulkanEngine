SET FLAGS=-target spirv -g

slangc.exe test_geometry_vert.slang %FLAGS% -o test_geometry_vert.spv -entry vertexMain
slangc.exe test_geometry_frag.slang %FLAGS% -o test_geometry_frag.spv -entry fragmentMain

pause