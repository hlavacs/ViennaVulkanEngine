slangc.exe T_vert.slang -target spirv -o T_vert.spv -entry vertexMain
slangc.exe T_frag.slang -target spirv -o T_frag.spv -entry fragmentMain

slangc.exe PNU_vert.slang -target spirv -o PNU_vert.spv -entry vertexMain
slangc.exe PNU_frag.slang -target spirv -o PNU_frag.spv -entry fragmentMain

slangc.exe PN_vert.slang -target spirv -o PN_vert.spv -entry vertexMain
slangc.exe PN_frag.slang -target spirv -o PN_frag.spv -entry fragmentMain
