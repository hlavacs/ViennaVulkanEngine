# AGENTS.md

## Purpose

This repository is building a reusable, ECS-first game meta engine in modern C++23.
The meta engine is meant for educational purposes, used in book projects or in lectures
and student projects. 

## Architecture

The meta engine is a container for concrete independent game engines and offers a common interface to 
user programmed games. If games restrict themselves to this common interface, then the meta engine
can be upgraded in the future to contain newer more modern game engines, and the
games then can opt to use them and still work without source code changes.

## Implementation Principles

The compiled outcome is a meta game engine, referred to as meta engine. It is a container for engine implementations, each being isoltaed from each other.

The common interface is defined in the src folder and does not contain any implementation itself, just interface contracts defining a facade facing towards the user program. This is the facade layer. The facade layer lives in namespace vve. User programs should only call into the facade layer.

The engine implementations are situated in the src/versions folder and are completely isolated from each other. This is the implementation layer. The meta engine can be compiled to contain all engine implementations or just one. Each game engine is isolated with its own namespace. For instance, v4 lives in the namespace vve::v4, source files are located in folder src/versions/v4.

When compiling their game, game apps are compiled and linked against the meta engine. Which engine implementation is then used is defined by the define VVE_ENGINE_IMPLEMENTATION_NAMESPACE. This define can be done as compiler parameter or as compiler präprocessor directive #define.
For instance, defining VVE_ENGINE_IMPLEMENTATION_NAMESPACE to be v4 results in using thr engine implementation living in namespace vve::v4.

Facades are defined in a facade pattern through wrapper classes and functions. Every class that is seen by the user lives in the facade layer as a wrapper. Wrappers have exactly one private member variable impl_ which is of type 
```cpp
  using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::<WRAPPED_CLASS>;
  Impl impl_{};
```
where wrapped class is a specific class of the implementation layer.
Wrappers mimic each method of the implementation, receive the same parameters and then forward them to the implementation. This way the contract is enforced and restricted to the allowed interface.

## Facade Structure

Engine implementations must expose specific subsystems with the enforced interface.

- Handle: Handles are strongly typed wrappers around uint64_t and id any resource.
- Math: Provides all math related functions.
- Vector: a basic vector like data container, requires iterators.
- Engine: This is created by the user app and handles main frame events.
- ECS: an Entity Component System that can hold entities with any data type.
- Window: Window information like number, renderer, camera, size.
- Assets: A wrapper over the internal asset system. The wrapper exposes specific public functions that enable users to load from disk and purge assets, create objects. 
- GUI: wrapper over the GUI system implementation. The wrapper offers public hooks for creating widgets. Must work with ImGUI. 
- World: main interface for user interaction with the world. 
  - Its API must offer enough member functions to access and change all world, scene and asset data without exposing internal descriptor types like SceneDescriptor or ObjectCatalogue. 
  - World contains references to: Asset wrapper, GUI wrapper, Engine wrapper, Windows wrapper. It allows for getting references to them.
  - World wrapper is a run time binder that binds all necessary subsystem wrappers into one class.
  - A reference to it can be obtained through a call to an Engine member function.
  - The world wrapper is actually created by the Engine impementation, which creates a World wrapper over the actual World implementation.

Additionally the facade defines numerous low level structs for storing pure data, like Position, 
Velocity, Orientation, etc. These must be used in the engine implementations accordingly and are typical data structures used in game engines.

Internal data are not part of the facade. The user states what he wants, 
the engine decides how this is done without exposing details about internal implementation. This also involves the containers storing these descriptors. 

Examples for internal data structures are 
- ObjectCatalogues
- SceneDescriptors
- NodeDescriptors used for creating internal DAGs or trees.

Descriptors that might be exposed to the user must be composed by facade defined data types only. However, as a general principle, user intercation should be via functions, not data structures.



## Strong Types

- Prefer strong types over raw primitives where semantics matter.
- Encode units, identifiers, handles, indices, ranges, states, flags, and categories as explicit types when practical.
- Avoid ambiguous `int`, `float`, `bool`, `string`, or loosely structured parameter lists in important APIs.
- Use types to prevent category mistakes, invalid combinations, and accidental misuse.
- Strong types should improve correctness without creating excessive ceremony on hot paths.
- Distinguish public semantic types from internal storage-efficient representations where needed.
- The name of a strong type should reflect its semantic meaning.


## External Depenencies

The engine mainly links to the official Vulkan SDK and uses the libraries contained there. Additonally it may use libraries like Assimp ot STB. These external libraries are downloaded using vcpkg.

### Code Documentation

- Add extensive Doxygen compatible comments to the code.
- Each file, function, class, struct gets a header explaining why it is there and if necessary input, output and return parameters.
- In a struct or enum or class, each member variable or value must have its own comment line at the end. 
- Function declarations should not have function header comments, only definitions.
- When adding comments, try to minimize the number of lines in the file. If max line length allows, put comments in the same line after the code.
- Add comments to roughly 30 to 50 percent of all code lines.
- If a new code block, e.g., a loop, begins, add comments in front of it to explain what the following code does.
- Keep comments simple and abstract. Prefer "what" to "how".
- In sequential lines, try to align comments vertically by adding tabs. 

### Modern C++ first

- Prefer modern C++ and the standard library by default.
- Use STL containers, algorithms, ranges, utilities, ownership models, `span`, `optional`, `variant`, `string_view`, concepts, and constexpr-oriented design where appropriate.
- Prefer standard facilities before introducing custom equivalents.
- Only replace STL or standard patterns when profiling, memory layout needs, platform constraints, or API constraints justify it.
- Optimize after understanding real hot paths, but design hot-path architecture correctly from the start.
- Prefer idiomatic modern C++ over C-style patterns or legacy inheritance-heavy OOP.
- Use templates deliberately, not decoratively.
- Prefer range-based for-loops over counted for-loops.


## Expectations for code generation

- Keep the number of lines as low as possible and feasible. 
- Do not bloat, do not introduce new classes or structs without asking.
- Do not violate the meta engine facade to engine implementation rules.
- Do not introduce layers of abstraction without asking.
- Do not introduce functionality that is already covered by std.
- Keeop the code readable, slim, expressive.
- Always document the code.
- If in doubt, ask. 


## Expectations for debugging

If tracking runtime errors try outputting internal data into a textfile, then 
fread the file to understand what was wring.


## Expectations for tests

All classes and functions should have unit tests.
All example programs should have extensive test paths.


