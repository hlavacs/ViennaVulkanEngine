# Vulkan Helpers

`vh` is a standalone C++ module-based Vulkan helper library extracted from useful v3 backend patterns.

The library has two layers:

- `vh.low` exports namespace `vh::low`, the lower layer. It contains stateless procedural helpers that use explicit standard C++ and Vulkan input/output data. These helpers plan instances, choose API versions, select devices, create logical devices, allocate buffers/images, query and create swapchains, build simple color/depth render passes, create framebuffers and frame sync primitives, allocate descriptor sets, and submit or record one-time command work without owning hidden state.
- `vh` exports namespace `vh`, the upper layer. It may own Vulkan handles, use RAII classes, collect data into structs, and call the lower layer.

The v3 `GraphicsBackend` uses `vh.low` for reusable Vulkan setup and teardown while keeping renderer-specific policy, draw recording, pipeline descriptions, and engine resource summaries in the backend.
