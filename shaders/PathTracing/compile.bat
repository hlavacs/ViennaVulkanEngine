@echo off

slangc vert.slang -entry main -target spirv -o vert.spv -DRASTERIZER

slangc frag.slang -entry main -target spirv -o frag.spv -DRASTERIZER

slangc combinePass.slang -entry main -target spirv -o combinePass.spv

slangc reprojectionPass.slang -entry main -target spirv -o reprojectionPass.spv

rem --- Raygen shader ---
slangc rtbasic.slang -entry rgenMain -target spirv -o raygen.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Direct_Raytracer.slang -entry rgenMain -target spirv -o raygen_direct.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirTemporal.slang -entry rgenMain -target spirv -o raygen_restir_temporal.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirSpatial.slang -entry rgenMain -target spirv -o raygen_restir_spatial.rgen.spv -DRAY_TRACING

rem --- Miss shader ---
slangc rtbasic.slang -entry rmissMain -target spirv -o miss.rmiss.spv -DRAY_TRACING

rem --- Closest hit shader ---
slangc rtbasic.slang -entry rchitMain -target spirv -o closesthit.rchit.spv -DRAY_TRACING

echo Done.
pause