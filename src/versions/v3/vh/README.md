# Vulkan Helpers

`vh` is a standalone C++ module-based Vulkan helper library extracted from useful v3 backend patterns without changing the current renderer path.

The library has two layers:

- `vh.low` exports namespace `vh::low`, the lower layer. It contains stateless procedural helpers that use explicit standard C++ and Vulkan input/output data. These helpers plan instances, choose API versions, select devices, create logical devices, allocate buffers/images, and submit one-time command buffers without owning hidden state.
- `vh` exports namespace `vh`, the upper layer. It may own Vulkan handles, use RAII classes, collect data into structs, and call the lower layer.

The existing v3 `GraphicsBackend` does not use this library yet. This keeps the current working renderer behavior unchanged while making reusable Vulkan infrastructure available for future renderers.
