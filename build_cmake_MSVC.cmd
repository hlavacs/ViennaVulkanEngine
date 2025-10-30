rem git clone -b nexgen.develop https://github.com/hlavacs/ViennaVulkanEngine.git
rem cd ViennaVulkanEngine

rem compile with MSVC
set CMAKE_BUILD_TYPE=Debug
cmake -S . -Bbuild -A x64 -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE%
cd build
cmake --build . --config %CMAKE_BUILD_TYPE%
doxygen
cd ..

rem run game from project directory!
rem build\examples\game\%CMAKE_BUILD_TYPE%\game.exe
