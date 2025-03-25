# The Vienna Vulkan Engine (VVE)
The Vienna Vulkan Engine (VVVE) is a Vulkan based render engine meant for learning and teaching the Vulkan API. It is open source under the MIT license. The VVE has been started as basis for game based courses at the Faculty of Computer Science of the University of Vienna, held by Prof. Helmut Hlavacs.

VVE's main contributor is Prof. Helmut Hlavacs (http://entertain.univie.ac.at/~hlavacs/). However, VVE will be heavily involved in the aforementioned courses, and other courses as well, and many students are already working on VVE extensions, porting, debugging etc.

VVE features are:
- 100% Vulkan, C++20
- Windowing through SDL2, other systems are possible.
- Multiplatform (almost) out of the box: Win 11, Linux, MacOS (using MoltenVK).
Build with cmake!

# Using Visual Studio Code

If you want to use Visual Studio Code (available for most platforms), install it for your platform. Also install the extensions Cmake an Cmake Tools. Then you can access all cmake features by clicking on Menu View / Command Palette... and enter cmake. You will be presented the cmake options which you can choose from in this sequence:
* Cmake: Select a Kit : Clang (Wundows, Linux) or AppleClang (MacOS)
* Cmake: Select Variant (Debug)
* Cmake: Configure
* Cmake: Build

For debugging select debug as variant, compile it, then choose the "Run and Debug" option on the left toolbar. In the drop down menu next to the green triangle choose your platform / compiler / variant:
* MSVC on Windows  - select "(Windows) debug"
* Clang on Windows - select "Clang Launch Test"
* Clang on MacOS - select "AppleClang Launch Test"
* Clang on Linux - select "Linux Clang Launch Test"

Click on the green triangle to start the program.

# Installing on Windows 

## Prerequisites:

You need these tools for compiling:
* Install MSVC and possibly Clang with it. When you compile from a command prompt, make sure to use "x64 Native Tools Command Prompt".
* It is recommended to ue Visual Studio Code, so install it also.
* Make sure you have CMake installed.
* Install the Vulkan SDK, the environment variable VULKAN_SDK needs to point to it. On Windows this should be done automatially.

## Compiling using CMake

If you do not want to use it, run the following commands in a "x64 Native Tools Command Prompt" from the project directory:

```
cmake -B build
cmake --build build --config Debug
build\Debug\28_model_loading.exe
```



# Installing on MacOS

1. **Install Dependencies:**
   - **Xcode:** Make sure you have Xcode installed on your macOS. You can download it from the Mac App Store.
   - **Homebrew:** If you don't have Homebrew installed, you can install it by running the following command in your terminal:
     ```sh
     /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
     ```

2. **Install Vulkan SDK:**
   - Download the Vulkan SDK for macOS from the [LunarG website](https://vulkan.lunarg.com/sdk/home).
   - Install it to a directory "VulkanSDK" in your home directory.

3. **Install MoltenVK:**
   - You can install MoltenVK using Homebrew:
     ```sh
     brew install moltenvk
     ```

4. **Set VULKAN_SDK**
   - Edit .zprofile in your home directory to let the environment variable VULKAN_SDK point to the VulkanSDK directory.
     ```sh
     export VULKAN_SDK="/Users/<YOURNAME>/VulkanSDK/1.4.309.0/macOS"
     ```
   - Log off and on again to apply the changes.

5. **Configuring and Compiling**
   - Use cmake directly to configure and compile
   - Or use MS Visual Studio Code, see below (recommended).




# Installing on Ubuntu

* Donwload the Vulkan SDK tar ball:
* Install xz to decompress the tarball:
* Install Visual Studio Code
* Install Clang:
--sudo apt install clang
--sudo apt-get install -y libc++-dev libc++abi-dev

