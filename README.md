# The Vienna Vulkan Engine (VVE)
The Vienna Vulkan Engine (VVVE) is a Vulkan based render engine meant for learning and teaching the Vulkan API. It is open source under the MIT license. The VVE has been started as basis for game based courses at the Faculty of Computer Science of the University of Vienna, held by Prof. Helmut Hlavacs:

- https://ufind.univie.ac.at/de/course.html?lv=052214&semester=2019W
- https://ufind.univie.ac.at/de/course.html?lv=052212&semester=2019W
- https://ufind.univie.ac.at/de/course.html?lv=052211&semester=2019S

VVE's main contributor is Prof. Helmut Hlavacs (http://entertain.univie.ac.at/~hlavacs/). However, VVE will be heavily involved in the aforementioned courses, and other courses as well, and many students are already working on VVE extensions, porting, debugging etc.

VVE features are:
- Vulkan 1.2, C++17
- Data oriented, the engine is essentially a relational database using its own VeTable class.
- VeTable provides unique handles, fast access through the handle, flexible ordering, cloning, memory alignment
- Can be easily used for uniform or storage buffers.
- Multithreading using own job system. The engine can be made single threaded for debugging.
- Windowing through GLFW, other systems are possible.
- Multiplatform (almost) out of the box: Win 10, Linux, MacOS (using MoltenVK).
- Uses several OSS libraries and  - if-possible - single-header libraries for loading assets, multithreading, etc.
- Documentation with Doxygen
- Simple GUI based on the Nuklear library, other libraries like ImGUI are possible.
- Uses AMD's VMA Library for memory allocation.


# Set up for Windows 10

Clone the project, open the .sln file, compile, run.
For Windows 10, all non-Vulkan dependencies are in the external directory. The Vulkan SDK is supposed to be pointed at by the VULKAN_SDK environment variable.

The project will be updated regularly, so it makes sense to download the source files once in a while, or clone the whole project. Make sure to keep your main.cpp or other files that you created.


## Links
-	https://github.com/hlavacs/ViennaVulkanEngine
-	https://stackoverflow.com/a/50728567/841052
-	https://vulkan-tutorial.com/Development_environment#page_MacOS
-	https://brew.sh
-	https://formulae.brew.sh/formula/glfw
-	https://formulae.brew.sh/formula/glm
-	https://formulae.brew.sh/formula/assimp
-	https://github.com/g-truc/gli
