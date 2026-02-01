@echo off

slangc vert.slang -entry main -target spirv -o vert.spv -DRASTERIZER

slangc frag.slang -entry main -target spirv -o frag.spv -DRASTERIZER


rem --- Raygen shader ---
slangc rtbasic.slang -entry rgenMain -target spirv -o raygen.rgen.spv -DRAY_TRACING

rem --- Miss shader ---
slangc rtbasic.slang -entry rmissMain -target spirv -o miss.rmiss.spv -DRAY_TRACING

rem --- Closest hit shader ---
slangc rtbasic.slang -entry rchitMain -target spirv -o closesthit.rchit.spv -DRAY_TRACING

echo Done.
pause