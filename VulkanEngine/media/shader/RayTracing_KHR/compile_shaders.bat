glslangValidator.exe --target-env vulkan1.2 -V raygen.rgen
glslangValidator.exe --target-env vulkan1.2 -V closesthit.rchit
glslangValidator.exe --target-env vulkan1.2 -V miss.rmiss
glslangValidator.exe --target-env vulkan1.2 -V shadow_miss.rmiss -o shadow_rmiss.spv
pause
