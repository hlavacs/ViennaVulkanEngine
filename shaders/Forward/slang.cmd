slangc.exe T_vert.slang -target spirv -o T_vert.spv -entry vertexMain
slangc.exe T_frag.slang -target spirv -o T_frag.spv -entry fragmentMain

slangc.exe C_vert.slang -target spirv -o C_vert.spv -entry vertexMain
slangc.exe C_frag.slang -target spirv -o C_frag.spv -entry fragmentMain
