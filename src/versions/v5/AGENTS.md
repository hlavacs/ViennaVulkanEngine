
# This file defines the AI rules for the v5 engine

## Development approach

- We apply a test driven approach. All tests report whether they succeeded. 
- Then one by one we implement functionality until the tests one by one succeed.
- The AI is not allowed to indiscriminately create code, structs, classes and functions.
- Data first - first we define the data structures, then the functions working on them.
- Only when there is a good plan with the data we declare functions.
- At this stage we do not fully define functions, but only make a plan which functions we need, what each function does, in, out and return values, and which functions are called by them.
- Once this is done we implement each function one by one. The AI is not allowed to just implement them. It needs the approval, then focus on one function only.
- Debugging is a first class activity, on par with development
- Debugging always means that there are ways to inspect what is going on on the GPU.
- Debugging requires to automatically download and dump any color or depth attachment as a file to the bin/verify folder and be inspected by the AI



## User interactions

- Create scene from file: all assets are loaded from file and all entities are automatically created  and rendered as decribed in the file
- Load scene from file: all assets are loaded from the file, but no entities are created yet
- Create entity: create single entity by refering to assets like mesh, textures, normal maps etc. that have been loaded from a file.
- Move/rotate/hide/unhide entities
- Erase entities
- Purge assets
Users should not have to explicitly deal with assets other than when creating an entity.


## Data-oriented design

- Prefer layouts and access patterns that support cache efficiency and predictable iteration.
- Keep hot-path data compact and easy to iterate.
- Separate hot runtime data from cold metadata, tooling data, and editor-facing structures where useful.
- Avoid mixing frequently updated simulation data with rarely used descriptive state.
- Favor batch processing, contiguous traversal, and explicit access patterns where appropriate.
- Design APIs and data structures so data flow is visible and cost is legible.
- Avoid object graphs or abstractions that destroy locality without strong justification.

## ECS-first architecture

- Core simulation architecture should be centered on entities, components, and systems.
- Entities should be lightweight identities.
- Components should primarily represent data.
- Systems should own behavior over component data.
- Avoid pushing business logic into entities or components.
- Prefer composition through components over inheritance hierarchies.
- System boundaries, scheduling, and data access patterns should be explicit.
- The architecture should make it easy to understand which systems read or write which component sets.
- ECS should remain legible and not collapse into a generic “everything bag” architecture.

## DAG-driven execution

- The execution of all frame tasks on the CPU is ordered through a DAG.
- Scheduling should be explicit, inspectable, and derivable from declared dependencies.
- Render work should be modeled through explicit dependency graphs rather than opaque sequencing.
- Engine task execution should prefer explicit dependency edges over ad hoc ordering conventions.
- DAGs should clarify execution and synchronization, not become ceremonial graph-building overhead.
- The dependency model should support future parallel execution, frame scheduling, and debugging.
- Systems that participate in scheduling should make read/write/resource dependencies visible.


## Reflection

- Reflection should be a first-class engine capability, designed intentionally rather than bolted on.
- Reflection should support engine needs such as:
  - serialization
  - editor/tooling integration
  - inspection/debugging
  - scripting interop
  - asset pipelines
  - metadata-driven workflows
  - creating the render graph from shader reflection data
- Reflection boundaries should be explicit.
- Do not smear reflection concerns through unrelated runtime code.
- Prefer reflection systems that preserve strong typing and compile-time structure where possible.
- Reflection metadata should not force runtime cost into hot paths without justification.


## Architecture

- Major engine concerns must remain separated, especially:
  - ECS core
  - task scheduling
  - rendering
  - render graph / frame graph
  - asset/resource management
  - physics is provided by a user system
  - audio
  - input
  - platform
  - serialization
  - reflection / metadata
  - scripting
  - tooling/editor
  - networking
 

## Vulkan API

Vulkan related code should be put into a library in folder vh. there should be two layers. A
stateful layer defines structs that store Vulkan related objects and uses the C++ interface. A
stateless layer consists only of true functions with input and output parameters, and it must not
access global state. The stateless layer must not receive or return structs from the stateful layer,
only Vulkan objects, handles, or data. This way the stateless layer remains portable and can be used
in other projects without enforcing the stateful layer structs.

Prefer Vulkan 1.4 and accept Vulkan 1.3 as the minimum modern target. Use dynamic rendering,
synchronization2, and timeline semaphores; avoid legacy render passes, framebuffers, and binary
frame-sync semaphores. Descriptor layouts must come from Slang reflection instead of arbitrary
hand-written descriptor sets. Prefer `VK_EXT_descriptor_buffer` as the descriptor-heap style path,
and fall back to descriptor indexing with update-after-bind, partially-bound bindings, variable
descriptor counts, and runtime descriptor arrays. Prefer the most modern readable approach because
v5 is an educational engine.
