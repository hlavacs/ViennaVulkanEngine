# The Vienna Vulkan Engine (VVE)
The Vienna Vulkan Engine (VVVE) is a Vulkan based render engine meant for learning and teaching the Vulkan API. It is open source under the MIT license. The VVE has been started as basis for game based courses at the Faculty of Computer Science of the University of Vienna, held by Prof. Helmut Hlavacs:

- https://ufind.univie.ac.at/de/course.html?lv=052214&semester=2019W
- https://ufind.univie.ac.at/de/course.html?lv=052212&semester=2019W
- https://ufind.univie.ac.at/de/course.html?lv=052211&semester=2019S

VVE's main contributor is Prof. Helmut Hlavacs (http://entertain.univie.ac.at/~hlavacs/). However, VVE will be heavily involved in the aforementioned courses, and other courses as well, and many students are already working on VVE extensions, porting, debugging etc.

VVE features are:
- 100% Vulkan, C++11, 14, 17, no templates
- Windowing through GLFW, other systems are possible.
- Multiplatform (almost) out of the box: Win 10, Linux, MacOS (using MoltenVK). See instructions below.
- Separation between the engine itself and an independent Vulkan helper layer below. The engine usually does not call Vulkan functions directly, but rather only helper functions, acting like macros. This reduces complexity.
- Simple callback usage through event listeners.
- Uses several OSS libraries and  - if-possible - single-header libraries for loading assets, multithreading, etc.
- Windowing through GLFW, other window systems are possible.
- Documentation with Doxygen
- Multiplatform (almost) out of the box: Win 10, Linux, MacOS (using MoltenVK). See instructions below.
- Separation between the engine itself and an independent Vulkan helper layer below. The engine usually does not call Vulkan functions directly, but rather only helper functions, acting like macros. This reduces complexity.
- Simple callback usage through event listeners.
- Uses several OSS libraries and - if possible - single-header libraries for loading assets, multithreading, etc.
- Simple GUI based on the Nuklear library, other libraries like ImGUI are possible.
- Uses AMD's VMA Library for memory allocation.


# Set up for Windows 10

Clone the project, open the .sln file, compile, run.
For Windows 10, all non-Vulkan dependencies are in the external directory. The Vulkan SDK is supposed to be pointed at by the VULKAN_SDK environment variable.

The project will be updated regularly, so it makes sense to download the source files once in a while, or clone the whole project. Make sure to keep your main.cpp or other files that you created.


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


# Setup of macOS branch in XCode
### Step 1: clone / import project to xCode
- cd to <project-dir>
- git clone —branch macOS https://github.com/hlavacs/ViennaVulkanEngine.git
- follow steps of this stack overflow post (https://stackoverflow.com/questions/18084928/import-project-from-git-in-xcode/50728567#50728567) to import project
- Select „Command Line Tool“ when creating the XCode project
- Important: When adding folders to Xcode using drag and drop, select “Copy items if needed” and select “Create groups” and mark the checkbox in „add to targets“

### Step 2: install dependencies, link frameworks and libraries
- most steps are documented in [this tutorial](https://vulkan-tutorial.com/Development_environment#page_MacOS)
- Download the Vulkan SDK: https://vulkan.lunarg.com/
- Install glfw3, glm and assimp using brew install
- Set up the Header Search Paths and Library search paths as described in the tutorial
- go to tab „Build Settings“
- add „/usr/local/include“ (for the libs installed by brew) and „<vulkansdk>/macOS/include“ to Header Search paths (set both to non-recursive) [<vulkansdk> := the path to the vulkan sdk on your machine]
- add the „external“ directory to header search paths (select „recursive“)
- remove everything from Library Search Paths (if there is anything)
- add „/usr/local/lib“ and „<vulkansdk>/macOS/lib“ to Library Search Paths
- go to tab „Build Phases“ (we’ll add the dynamic libraries for vulkan / glfw3 / assimp now)
- remove everything from the List „Link Binary with Frameworks“
- from /usr/local/lib“, drag „libassimp.5.x.x.dylib“ & „libglfw3.x.dylib“ into Linked Frameworks and Libraries
- from <vulkansdk>/macOS/lib, drag „libvulkan.1.dylib“ and „libvulkan.1.x.xx.dylib“ 
- Note: the „x“’s depend on the version of the libraries that you have installed ]
- in Build Phases -> „Copy Files“, change Destination to "Frameworks", clear the subpath and deselect "Copy only when installing". Click on the "+" sign and add all frameworks here aswell.
- Add the Metal Library: in „Link Binary With Libraries“, click ‚+‘, search for Metal.framework
- Add the .cpp files from the project to the “Compile Sources” Build Phase (if they aren’t there by default)
--> Open /VulkanEngine/VHHelper.h. If there are no errors in this file, all libs/frameworks are found.

# Setup of macOS branch with Clion
- Download the Vulkan SDK and unzip to whatever folder you want for example "~/Code/CLionProjects/vulkanSDK/"
- Run script install_vulkan.py  
- install glfw3, glm and assimp using brew install
- go to file VulkanEngine/CMakeLists.txt and at the Top click on appeared button "reload changes". After that CTRL+R should run compilation. 
- or you can setUp configuration manually just create new conf -> CMake App (target app, executable app)
- change "working directory" to "~/Code/CLionProjects/ViennaVulkanEngine/VulkanEngine" run/debug config -> edit config -> working directory.