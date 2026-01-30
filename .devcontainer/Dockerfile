FROM ubuntu:24.04

ARG VulkanSDKVersion=1.4.335.0
ARG SDL3TagOrCommit=release-3.4.0

WORKDIR /app

# Install basic build tools
RUN apt-get update && apt-get install -y \
    curl \
    xz-utils \
    cmake \
    ninja-build \
    gdb

# Install other dependencies
RUN apt-get install -y \
    # CMake dependencies
    git pkg-config \
    # SDL3 dependencies (https://wiki.libsdl.org/SDL3/README-linux#build-dependencies)
    cmake ninja-build gnome-desktop-testing libasound2-dev libpulse-dev \
    libaudio-dev libfribidi-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev \
    libpipewire-0.3-dev libwayland-dev libdecor-0-dev liburing-dev \
    # SDL3 dependencies not mentioned in the README
    libusb-1.0-0-dev \
    # SDL3_mixer dependencies
    libogg-dev libopus-dev libopusfile-dev libgme-dev libxmp-dev libfluidsynth-dev fluidsynth libwavpack-dev \
    # Other VVE dependencies
    libglm-dev \
    # Documentation tools
    doxygen graphviz

# During configuration, CMake can't find libunwind - don't know why. It works without it.
RUN apt-get install -y \
    build-essential \
    clang-20 libc++-20-dev libc++abi-20-dev

ENV XDG_RUNTIME_DIR=/run/user

# Download, build and install SDL3 - needed for VECS Console on Linux
RUN git clone -b ${SDL3TagOrCommit} --depth 1 https://github.com/libsdl-org/SDL.git /tmp/SDL3
WORKDIR /tmp/SDL3
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja && \
    cmake --build build && cmake --install build --prefix /usr/local
WORKDIR /app
RUN rm -rf /tmp/SDL3

# Download Vulkan SDK
RUN curl -o /tmp/vulkansdk.tar.gz https://sdk.lunarg.com/sdk/download/${VulkanSDKVersion}/linux/vulkansdk-linux-x86_64-${VulkanSDKVersion}.tar.xz
RUN mkdir /opt/VulkanSDK && tar -xf /tmp/vulkansdk.tar.gz -C /opt/VulkanSDK && rm /tmp/vulkansdk.tar.gz
RUN echo "source /opt/VulkanSDK/${VulkanSDKVersion}/setup-env.sh" >> ~/.bashrc
ENV VULKAN_SDK=/opt/VulkanSDK/${VulkanSDKVersion}/x86_64
RUN echo "VULKAN_SDK=${VULKAN_SDK}"

# Clean up download related packages
RUN apt-get remove -y curl xz-utils