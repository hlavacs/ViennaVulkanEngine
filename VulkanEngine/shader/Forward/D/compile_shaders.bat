glslangValidator.exe -V shader.vert
glslangValidator.exe -DALL -V shader.frag
glslangValidator.exe -DSPOT  -o frag_SPOT.spv -V shader.frag
glslangValidator.exe -DDIR   -o frag_DIR.spv -V shader.frag
glslangValidator.exe -DPOINT -o frag_POINT.spv -V shader.frag
pause
