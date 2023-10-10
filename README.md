# The Vienna Vulkan Engine (VVE)
The Vienna Vulkan Engine (VVVE) is a Vulkan based render engine meant for learning and teaching the Vulkan API. It is open source under the MIT license. The VVE has been started as basis for game based courses at the Faculty of Computer Science of the University of Vienna, held by Prof. Helmut Hlavacs:

- https://ufind.univie.ac.at/de/course.html?lv=052212&semester=2023W
- https://ufind.univie.ac.at/de/course.html?lv=052211&semester=2023S
- https://ufind.univie.ac.at/de/course.html?lv=052214&semester=2023S

VVE's main contributor is Prof. Helmut Hlavacs (http://entertain.univie.ac.at/~hlavacs/). However, VVE will be heavily involved in the aforementioned courses, and other courses as well, and many students are already working on VVE extensions, porting, debugging etc.

VVE features are:
- 100% Vulkan, C++20
- Windowing through GLFW, other systems are possible.
- Multiplatform (almost) out of the box: Win 10, Linux, MacOS (using MoltenVK). See instructions below.
- Separation between the engine itself and an independent Vulkan helper layer below. The engine usually does not call Vulkan functions directly, but rather only helper functions, acting like macros. This reduces complexity.
- Simple callback usage through event listeners.
- Uses several OSS libraries and  - if-possible - single-header libraries for loading assets, multithreading, etc.
- Documentation with Doxygen
- Multiplatform (almost) out of the box: Win 11, Linux, MacOS (using MoltenVK). See instructions below.
- Separation between the engine itself and an independent Vulkan helper layer below. The engine usually does not call Vulkan functions directly, but rather only helper functions, acting like macros. This reduces complexity.
- Simple callback usage through event listeners.
- Uses several OSS libraries and - if possible - single-header libraries for loading assets, multithreading, etc.
- Simple GUI based on the Nuklear library, other libraries like ImGUI are possible.
- Uses AMD's VMA Library for memory allocation.


# How to use Vienna Vulkan Engine

VVE lets you load assets from disk, create arbitrary scene graphs, and render objects with shadows. See the examples folder for a simple example game.


# Set up for Windows 11

The VVE must be compiled into a static link library, under Windows 11 this is a lib file. Do the following:
- Make sure you have an up to date CMake, MS Visual Studio 22 and the Vulkan SDK installed.
- Clone both projects Vienna Vulkan Engine and Vienna Physics Engine, into the same directory, next to each other.
- Cd into Vienna Vulkan Engine and run CMake. Alternatively run the msvc bat file. CMake creates an sln project file.
- Open the sln file and compile the project. Sometimes compile it twice to make sure everything is done correctly.
- You might also manually compile the doxyfile subproject to create the documentation. For this you must have Doxygen installed.

The project will be updated regularly, so it makes sense to pull the newest version regularly.


# Set up environment for Ubuntu 18.04 (Thanks to Lukas Walisch)

dependencies:

- Vulkan SDK, GLFW, Assimp

https://vulkan-tutorial.com/Development_environment#page_Linux
Follow the tutorial to install Vulkan and GLFW. At the end the header files and the libraries from Vulkan and GLFW should be in the path.

- Assimp

```
sudo apt install libassimp-dev
```

- GLM

```
sudo apt install libglm-dev
```

- stb_image.h
Download the file from https://github.com/nothings/stb. No compilation required. Just put the stb_image.h file in the path (e.g. `mv stb_image.h /usr/local/include`)

- GLI
Download the library from https://github.com/g-truc/gli. IMPORTANT: Download from the master branch, NOT the latest release. Extract the directory, open a terminal in the downloaded directory and compile the library with
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
Info: I use the CLion IDE for developing. It is a minimal effort to set up the project as CLion only needs a CmakeLists.txt file which already exists. If you add new source files, be sure to add them to CmakeLists.txt. Debugging with breakpoints is possible.


# Vienna Vulkan Engine macOS Instructions (Thanks to Emanuel Stadler)

## Import project from GitHub
-	To import the project from git into Xcode, follow the steps described in this Stack Overflow answer: https://stackoverflow.com/a/50728567/841052
-	When adding folders to Xcode using drag and drop, select “Copy items if needed” and select “Create groups”.
-	The external folder and the VulkanEngine folder need to be added to Xcode
-	The VulkanSDK folder can be deleted in Xcode, as we are using the macOS SDK

## Set up Xcode
-	We follow the steps described on https://vulkan-tutorial.com/Development_environment#page_MacOS
-	Download the Vulkan SDK
-	Install glfw3, glm and assimp using brew install (Homebrew is at: https://brew.sh)
-	Set up the Header Search Paths and Library search paths as described in the tutorial
-	Add the external directory to the Header Search Paths and select recursive
-	Add the glfw3, libassimp and libvulkan libraries in the Build Phases to Link Binary With Libraries as described in the tutorial
-	If you want to debug shaders, include the Metal framework in Link Binary With Libraries
-	Add the .cpp files from the project to the “Compile Sources” Build Phase
-	Add the environment variables as described in the tutorial (Edit Scheme)
-	In Edit Scheme > Options, use custom working directory and select the directory “VulkanEngine”

## Source Code
-	You might need to edit VHHelper.h lines 37 and 38 (stb) to use quotes instead of angled brackets.s
-	You might need to download the current version of gli-master from the master branch on https://github.com/g-truc/gli and replace the version in the external directory or alternatively change line 81 in gli/core/bc.inl from uint_t Channel1 to uint8_t Channel1 (see also https://github.com/g-truc/gli/issues/154)
Building and running
-	In main.cpp on line 165 set mve(true) to mve(false) to disable debugging and the validation layers, otherwise the engine does not work.
-	The project should now build and run on macOS, you might need to clean your build folder before building.

## Links
-	https://github.com/hlavacs/ViennaVulkanEngine
-	https://stackoverflow.com/a/50728567/841052
-	https://vulkan-tutorial.com/Development_environment#page_MacOS
-	https://brew.sh
-	https://formulae.brew.sh/formula/glfw
-	https://formulae.brew.sh/formula/glm
-	https://formulae.brew.sh/formula/assimp
-	https://github.com/g-truc/gli
