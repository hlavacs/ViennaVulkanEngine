rem git clone -b nexgen.develop https://github.com/hlavacs/ViennaVulkanEngine.git
rem cd ViennaVulkanEngine

rem compile with Clang
rem cmake -S . -Bbuild -G "Visual Studio 17 2022" -T ClangCL -A x64
cmake -G "Visual Studio 17 2022" -A x64 -T ClangCL -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl++ -S . -Bbuild -DCMAKE_BUILD_TYPE=Debug
cd build
cmake --build . --config Debug

rem compile with MSVC
rem cmake -S . -Bbuild -A x64
rem cd build
rem cmake --build . --config Release
rem ctest -C Release
rem cmake --build . --config Debug
rem ctest -C Debug
rem cd ..

doxygen

