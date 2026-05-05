
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


  