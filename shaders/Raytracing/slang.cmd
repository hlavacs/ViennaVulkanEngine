slangc.exe raygen.slang -target spirv -o raygen.rgen.spv -entry rgenMain
slangc.exe miss.slang -target spirv -o miss.rmiss.spv -entry rmissMain
slangc.exe closesthit.slang -target spirv -o closesthit.rchit.spv -entry rchitMain
