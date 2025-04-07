SET FLAGS=-target spirv -g

slangc.exe Common.slang %FLAGS% -o Common.spv
slangc.exe Shadow11.slang %FLAGS% -o Shadow11.spv
slangc.exe 0100_PNUTE.slang %FLAGS% -o 0100_PNUTE.spv
slangc.exe 1000_PNC.slang %FLAGS% -o 1000_PNC.spv
slangc.exe 2000_PNO.slang %FLAGS% -o 2000_PNO.spv
