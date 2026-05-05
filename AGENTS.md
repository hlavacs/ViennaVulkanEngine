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
- Assets: Management of any 3D or audio related resource on a high abstract level.
- Window: Window information like number, renderer, camera, size.
- World: main interface for user interaction with the world.


## Strong Types

- Prefer strong types over raw primitives where semantics matter.
- Encode units, identifiers, handles, indices, ranges, states, flags, and categories as explicit types when practical.
- Avoid ambiguous `int`, `float`, `bool`, `string`, or loosely structured parameter lists in important APIs.
- Use types to prevent category mistakes, invalid combinations, and accidental misuse.
- Strong types should improve correctness without creating excessive ceremony on hot paths.
- Distinguish public semantic types from internal storage-efficient representations where needed.


## External Depenencies

The engine mainly links to the official Vulkan SDK and uses the libraries contained there. Additonally it may use libraries like Assimp ot STB. These external libraries are downloaded using vcpkg.

### Code Documentation

- Add extensive Doxygen compatible comments to the code.
- Each file, function, class, struct gets a header explaining why it is there and if necessary input, output and return parameters.
- In a struct or enum or class, each member variable or value must have its own comment line at the end. 
- Function declarations should not have comments, only definitions.
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
- Do not violate the meta engine to engine implementation rules.
- Do not introduce layers of abstraction wihtout asking.
- Do nto introduce functionality that is covered by std.
- Keeop the code readable, slim, expressive.
- Always document the code.
- If in doubt, ask. 


## Expectations for debugging

When debugging:

1. state the observed symptom
2. identify the relevant subsystem boundary
3. trace the likely execution and data path
4. identify ownership/lifecycle assumptions involved
5. distinguish root cause from downstream effects
6. propose the smallest fix that addresses the cause
7. mention architectural follow-up if the bug points to a systemic design weakness

For engine bugs, be especially alert to:

- invalid lifetime assumptions
- ordering problems across phases
- resource invalidation
- stale handles/references
- structural ECS mutation issues
- synchronization issues
- hidden subsystem coupling
- API ambiguity
- backend/state desynchronization
- performance regressions caused by innocent-looking abstractions

Do not treat every bug as grounds for a large rewrite.


## Expectations for tests

When adding or updating tests:

- test observable behavior and contracts
- test subsystem boundaries and API semantics
- add focused regression tests for the actual bug or risk
- test ECS invariants and lifecycle expectations where relevant
- prefer tests that validate ownership/lifetime behavior when that is part of correctness
- avoid excessive mocking that hides architecture problems
- test extension points when introducing them
- test patterns and contracts, not incidental implementation trivia
- include performance-sensitive tests or benchmarks where performance is a design requirement

If testing is difficult because architecture is unclear or too coupled, say so explicitly and identify the boundary problem.


