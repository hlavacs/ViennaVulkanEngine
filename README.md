# The Vienna Vulkan Engine
A Vulkan based render engine. This engine is meant for learning and teaching the Vulkan API. It is open source under the MIT license.


# Set up for Windows 10

Clone the project, open the .sln file, compile, run.
For Windows 10, all dependencies (including the Vulkan SDK) are in the external directory.
The project will be updated regularly, so it makes sense to download the source files once in a while, or clone the whole project.
Make sure to keep your main.cpp or other files that you created.


# Set up environment for Ubuntu 18.04 (Thanks to Lukas Walisch)

dependencies:

- Vulkan sdk & GLFW

https://vulkan-tutorial.com/Development_environment#page_Linux
Follow the tutorial to install Vulkan and GLFW. At the end the header files and the libraries from Vulkan and GLFW
should be in the path.

- Assimp

```
sudo apt install libassimp-dev
```

- GLM

```
sudo apt install libglm-dev
```

- stb_image.h
Download the file from https://github.com/nothings/stb. No compilation required. Just put the stb_image.h file in the
path (e.g. `mv stb_image.h /usr/local/include`)

- GLI
Download the library from https://github.com/g-truc/gli. IMPORTANT: Download from the master branch, NOT the latest
release. Extract the directory, open a terminal in the downloaded directory and compile the library with
```
cmake .
make
sudo make install
```

## Run the project
All whats left is to run the project using cmake. Open a terminal in the project root and build the executable
```
cmake .
make
```

and run the executable with
```
./vienna_vulkan_engine_cmake
```

## Development
Info: I use the CLion IDE for developing. It is a minimal effort to set up the project as CLion only needs a CmakeLists.txt
file which already exists. Debugging with breakpoints is possible.
