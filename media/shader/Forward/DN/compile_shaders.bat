glslangValidator.exe -V shader.vert
glslangValidator.exe -DALL -DSPOT -DDIR -DPOINT -DAMB -V shader.frag
rem glslangValidator.exe -DSPOT  -o frag_SPOT.spv -V shader.frag
rem glslangValidator.exe -DDIR   -o frag_DIR.spv -V shader.frag
rem glslangValidator.exe -DPOINT -o frag_POINT.spv -V shader.frag
rem glslangValidator.exe -DAMB -o frag_AMB.spv -V shader.frag
pause
