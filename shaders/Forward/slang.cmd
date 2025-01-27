slangc.exe Common.slang -target spirv -o Common.spv

slangc.exe PNUT_vert.slang -target spirv -o PNUT_vert.spv -entry vertexMain
slangc.exe PNUT_frag.slang -target spirv -o PNUT_frag.spv -entry fragmentMain

slangc.exe PNC_vert.slang -target spirv -o PNC_vert.spv -entry vertexMain
slangc.exe PNC_frag.slang -target spirv -o PNC_frag.spv -entry fragmentMain
