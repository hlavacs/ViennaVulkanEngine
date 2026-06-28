# AGENTS.md

## src/versions/simple

The simple engine should be the bare engine minimum. Just to render objects, and have lights and shadows. 
- Use scene graph similar to v5. Do not use render or task graphs.
- Use v5 helper classes like Handle, Vector, Graph, ECS, Math instead of reimplementing them.
- Create empty source files as soon as possible.
- No virtual layer, keep things explicit for the time being.
- Keep the code as simple as possible. 
- Always prefer STL, be it containers, algorithms, etc. Use STL instead of new classes if possible.
- Keep number of new types, structs at a minumum.
- The best class is the class not needed. The same is true for structs, types, enums, functions, etc.
- The engine should mirror the official Vulkan tutorial https://github.com/KhronosGroup/Vulkan-Tutorial (en/16_Multiple_Objects) in structure.
- It should use SDL3, VMA, Assimp, Slang, dynamic rendering, Vulkan profiles. 
- It should enable loadig assets and rendering multiple objects.
- Basis is Specification 1.4.
- It should provide a simple, minimal debugging layer that transports debugging information for Slang, SDL3 windows, rendering output. 
- It should use the light shadow debug example from v5 that produces some known output that can stored in a PNG for automatic analysis.
- The debugging example light shadow debug should not be part of the simple engine, but be in the examples folder. 
- The LLM should ingest this output and analyse whether the current version produces the correct result.
- The goal is to let an LLM do the heavy lifiting, to the LLM should be able to reliably detect bugs through the debuggin output.
- Debugging information that is collected and moved e.g. from GPU to CPU should be kept at a minimum necessary. 
- Add code comments and function and class headers. 
- At the start of each file provide an overviw over the functional objects (classes, enums, types, structs, ...) and their purpose in this file.
- The engine should not use Python anywhere, also not for testign. It can use cmd or bash scripts.






