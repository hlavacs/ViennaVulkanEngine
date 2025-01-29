slangc.exe Common.slang -target spirv -o Common.spv

slangc.exe S0100_PNUT_vert.slang -target spirv -o S0100_PNUT_vert.spv -entry vertexMain
slangc.exe S0100_PNUT_frag.slang -target spirv -o S0100_PNUT_frag.spv -entry fragmentMain

slangc.exe S1000_PNC_vert.slang -target spirv -o S1000_PNC_vert.spv -entry vertexMain
slangc.exe S1000_PNC_frag.slang -target spirv -o S1000_PNC_frag.spv -entry fragmentMain
