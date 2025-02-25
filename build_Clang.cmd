rem git clone -b nexgen.develop https://github.com/hlavacs/ViennaVulkanEngine.git
rem cd ViennaVulkanEngine

rem compile with Clang
set BUILD_TYPE=Debug
cmake -S . -Bbuild -A x64 -G "Visual Studio 17 2022" -T ClangCL -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl++ -Bbuild -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
cd build
cmake --build . --config %BUILD_TYPE%
doxygen
cd ..
