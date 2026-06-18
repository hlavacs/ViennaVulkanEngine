slangc.exe raygen.slang -target spirv -o raygen.rgen.spv -entry rgenMain
slangc.exe miss.slang -target spirv -o miss.rmiss.spv -entry rmissMain
slangc.exe shadowmiss.slang -target spirv -o shadowmiss.rmiss.spv -entry rmissShadowMain
slangc.exe closesthit.slang -target spirv -o closesthit.rchit.spv -entry rchitMain
