SET FLAGS=-target spirv -g

slangc.exe Common.slang %FLAGS% -o Common.spv

slangc.exe Shadow11_vert.slang %FLAGS% -o Shadow11_vert.spv -entry vertexMain
slangc.exe Shadow11_geom.slang %FLAGS% -o Shadow11_frag.spv -entry geometryMain
slangc.exe Shadow11_frag.slang %FLAGS% -o Shadow11_frag.spv -entry fragmentMain

slangc.exe 0100_PNUTE_vert.slang %FLAGS% -o 0100_PNUTE_vert.spv -entry vertexMain
slangc.exe 0100_PNUTE_frag.slang %FLAGS% -o 0100_PNUTE_frag.spv -entry fragmentMain

slangc.exe 1000_PNC_vert.slang %FLAGS% -o 1000_PNC_vert.spv -entry vertexMain
slangc.exe 1000_PNC_frag.slang %FLAGS% -o 1000_PNC_frag.spv -entry fragmentMain

slangc.exe 2000_PNO_vert.slang %FLAGS% -o 2000_PNO_vert.spv -entry vertexMain
slangc.exe 2000_PNO_frag.slang %FLAGS% -o 2000_PNO_frag.spv -entry fragmentMain
