@echo off

slangc vert.slang -entry main -target spirv -o vert.spv

slangc frag.slang -entry main -target spirv -o frag.spv


rem --- Raygen shader ---
slangc rtbasic.slang -entry rgenMain -target spirv -o raygen.rgen.spv

rem --- Miss shader ---
slangc rtbasic.slang -entry rmissMain -target spirv -o miss.rmiss.spv

rem --- Closest hit shader ---
slangc rtbasic.slang -entry rchitMain -target spirv -o closesthit.rchit.spv

echo Done.
pause