# The Vienna Vulkan Engine (VVE) - Linux
This README describes how to set up the dev enviroment on Linux via VSCode and Docker.

> [!NOTE]
> Only tested with an AMD GPU and X11 on Linux Mint/Ubuntu. Also works with an Intel iGPU but fullscreening may be broken.
> 
> With an NVIDIA GPU you might need the [nvidia-container-toolkit](<https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html>) locally installed and need to edit `.devcontainer/devcontainer.json` and replace `--device=/dev/dri` with `--gpus all`.
> 
> For native Wayland you may ask an AI of your choice to assist you.
> 
> Also audio may or may not work depending on your setup - it worked fine for me with PipewWire as audio server.

## Prerequesites
* [VSCode](<https://code.visualstudio.com/>)
* [Docker Engine](<https://docs.docker.com/engine/install/>) - NOT Docker Desktop!!
* Be in the `docker` group to use docker commands without root
* [Dev Container Extension](<https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers>)

## Setup
After cloning and opening the project in VSCode open the Command Pallete (`F1`) and use the `>Dev Containers: Rebuild and Reopen in Container` command to build the development container and open the project inside it.

Once you are in the container you can check if the GPU is detected by executing
```bash
vulkaninfo | head -n 60
```
> [!NOTE]
> If you get an output like this: `Authorization required, but no authorization protocol specified`
> 
> Then grant permission to the root user to use your X11 session - RUN ON LOCAL USER, NOT IN THE CONTAINER!:
> ```bash
> xhost +si:localuser:root
> ```
> You can remove the permissions again with
> ```bash
> xhost -si:localuser:root
> ```

You can now select the right compiler with `>CMake: Select a Kit` - I use *Clang \[...\]* (the one without *-cl*). If the list is empty select the *\[Scan for Kits\]* option and invoke the command again.

And use `>CMake: Configure` and `>CMake: Build`. To change the build type use `>CMake: Select Variant` (e.g. *Debug* or *Release*).

To debug/run an example use the *Run and Debug* (`CTRL+SHIFT+D`) option in the sidebar and select the appropriate option or create your own by modifying `.vscode/launch.json`.