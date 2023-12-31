cmake -S . -Bbuild
cd build
cmake --build . --config Release
ctest -C Release
cmake --build . --config Debug
ctest -C Debug
cd ..
