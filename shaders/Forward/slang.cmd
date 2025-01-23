slangc.exe T_vert.slang -target spirv -o T_vert.spv -entry vertexMain
slangc.exe T_frag.slang -target spirv -o T_frag.spv -entry fragmentMain

slangc.exe PNUT_vert.slang -target spirv -o PNUT_vert.spv -entry vertexMain
slangc.exe PNUT_frag.slang -target spirv -o PNUT_frag.spv -entry fragmentMain

slangc.exe PN_vert.slang -target spirv -o PN_vert.spv -entry vertexMain
slangc.exe PN_frag.slang -target spirv -o PN_frag.spv -entry fragmentMain

slangc.exe PNC_vert.slang -target spirv -o PNC_vert.spv -entry vertexMain
slangc.exe PNC_frag.slang -target spirv -o PNC_frag.spv -entry fragmentMain
