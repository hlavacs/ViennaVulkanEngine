SET FLAGS=-target spirv -g

slangc.exe Common.slang %FLAGS% -o Common.spv

slangc.exe Shadow11_vert.slang %FLAGS% -o Shadow11.spv -entry vertexMain
slangc.exe Shadow11_frag.slang %FLAGS% -o Shadow11.spv -entry fragmentMain

slangc.exe S0100_PNUTE_vert.slang %FLAGS% -o S0100_PNUTE_vert.spv -entry vertexMain
slangc.exe S0100_PNUTE_frag.slang %FLAGS% -o S0100_PNUTE_frag.spv -entry fragmentMain

slangc.exe S1000_PNC_vert.slang %FLAGS% -o S1000_PNC_vert.spv -entry vertexMain
slangc.exe S1000_PNC_frag.slang %FLAGS% -o S1000_PNC_frag.spv -entry fragmentMain

slangc.exe S2000_PNO_vert.slang %FLAGS% -o S2000_PNO_vert.spv -entry vertexMain
slangc.exe S2000_PNO_frag.slang %FLAGS% -o S2000_PNO_frag.spv -entry fragmentMain
