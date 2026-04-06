# AGENTS.md

## Purpose

This repository is building a reusable, ECS-first game engine in modern C++.

Primary priorities, in order:

1. performance
2. build once, reuse many times
3. minimal technical debt from the start
4. stable, slim, hard-to-misuse APIs
5. extendability through clear subsystem facades and explicit boundaries
6. strong separation of concerns
7. compile-time guarantees where practical
8. data-oriented design
9. maintainable long-term architecture

All analysis, design, and implementation should optimize for an engine that can serve as a long-lived reusable foundation across many projects, not for one-off convenience or game-specific shortcuts.

Default operating mode:

1. understand the existing subsystem
2. identify responsibilities, boundaries, ownership, data flow, compile-time/runtime tradeoffs, and runtime costs
3. evaluate design quality against the standards below
4. propose a minimal, coherent plan
5. make the smallest useful change consistent with long-term engine reuse

Do not jump directly into code generation unless explicitly asked.

---

## Core engineering principles

### Reusable engine first

- The engine is a reusable foundation, not an individual game codebase.
- Prefer general engine capabilities over game-specific assumptions.
- Avoid embedding game rules, content assumptions, genre-specific logic, or project-specific workflows into core engine systems.
- Design for reuse across multiple projects with different gameplay needs.
- Prefer stable extension seams over ad hoc customization.
- Every major subsystem should be usable without requiring modification to unrelated systems.
- Project-specific code should live on top of the engine, not inside core engine layers.

### Minimize technical debt from the start

- Do not knowingly introduce shortcuts that are likely to harden into technical debt.
- Prefer a correct and extensible subsystem boundary now over a quick local fix that distorts the architecture.
- Avoid placeholder abstractions unless they already match the intended long-term shape.
- Avoid “temporary” hacks in core engine code.
- Solve root architectural issues early when they are still cheap to solve.
- Prefer explicitness, ownership clarity, strong boundaries, and correct layering over convenience.
- Keep incidental complexity out of the codebase from the beginning.

### Separation of concerns

- Each subsystem should have a narrow, explicit responsibility.
- Major engine concerns must remain separated, especially:
  - ECS core
  - task scheduling
  - rendering
  - render graph / frame graph
  - asset/resource management
  - physics
  - audio
  - input
  - platform
  - serialization
  - reflection / metadata
  - scripting
  - tooling/editor
  - networking
- Avoid mixing orchestration, storage, policy, synchronization, and execution in the same layer.
- Avoid leaking rendering details into ECS core, editor concerns into runtime, or game-specific concerns into engine subsystems.
- Keep subsystem dependencies directional and intentional.

### Extendability through facades

- Prefer extendability through clear subsystem facades.
- A facade should expose a stable, slim, high-level contract for a subsystem.
- A facade should hide incidental implementation details, not erase important concepts.
- Facades should exist at subsystem boundaries, not as all-knowing global interfaces.
- Facades must not become god objects.
- Internal implementation may evolve behind a facade without breaking consumers.
- A facade should make intended use obvious and misuse difficult.
- Lower-level APIs may exist behind the facade for internal engine use where necessary, but must remain contained.

### Slim APIs you cannot abuse

- Public APIs must be narrow, explicit, and difficult to misuse.
- Prefer APIs that encode correct usage in the type system or lifecycle model.
- Expose engine concepts, not storage accidents or implementation quirks.
- Avoid “do everything” interfaces.
- Avoid APIs that require callers to know hidden sequencing rules.
- Avoid convenience APIs that silently allocate, synchronize, invalidate state, or trigger expensive work.
- Make ownership, lifetime, mutability, and cost visible.
- Prefer fewer, stronger API entry points over many weak ones.
- Default to API designs that are restrictive in the good sense: obvious, safe, and hard to abuse.

### Strong types

- Prefer strong types over raw primitives where semantics matter.
- Encode units, identifiers, handles, indices, ranges, states, flags, and categories as explicit types when practical.
- Avoid ambiguous `int`, `float`, `bool`, `string`, or loosely structured parameter lists in important APIs.
- Use types to prevent category mistakes, invalid combinations, and accidental misuse.
- Strong types should improve correctness without creating excessive ceremony on hot paths.
- Distinguish public semantic types from internal storage-efficient representations where needed.

### Compile-time first

- Prefer compile-time validation over runtime validation where practical.
- Prefer expressing invariants through types, templates, concepts, `constexpr`, and static checks when this improves correctness, safety, or API quality.
- Shift errors to compile time where doing so improves reliability and does not distort the design.
- Use templates where they provide real leverage in zero-cost abstraction, policy selection, strong typing, or compile-time composition.
- Avoid template complexity that harms legibility, diagnostics, build times, or API stability without strong benefit.
- Keep compile-time machinery justified and comprehensible.
- Favor compile-time dispatch and structure where it meaningfully reduces runtime overhead.
- Prefer templates over virtual functions.

### Modern C++ first

- Prefer modern C++ and the standard library by default.
- Use STL containers, algorithms, ranges, utilities, ownership models, `span`, `optional`, `variant`, `string_view`, concepts, and constexpr-oriented design where appropriate.
- Prefer standard facilities before introducing custom equivalents.
- Only replace STL or standard patterns when profiling, memory layout needs, platform constraints, or API constraints justify it.
- Optimize after understanding real hot paths, but design hot-path architecture correctly from the start.
- Prefer idiomatic modern C++ over C-style patterns or legacy inheritance-heavy OOP.
- Use templates deliberately, not decoratively.
- Prefer range-based for-loops over counted for-loops.

### Performance first

- Performance is a primary design constraint, not an afterthought.
- Optimize architecture for predictable runtime behavior, data locality, controlled allocation, and low-overhead execution.
- Avoid abstractions that obscure cost on hot paths.
- Make expensive operations visible in API shape and naming.
- Avoid hidden allocations, unnecessary virtual dispatch in critical paths, avoidable indirection, and implicit synchronization.
- Prefer designs that make frame cost understandable and measurable.
- Do not sacrifice architectural clarity casually, but treat performance-sensitive paths as first-class.

### ECS-first architecture

- Core simulation architecture should be centered on entities, components, and systems.
- Entities should be lightweight identities.
- Components should primarily represent data.
- Systems should own behavior over component data.
- Avoid pushing business logic into entities or components.
- Prefer composition through components over inheritance hierarchies.
- System boundaries, scheduling, and data access patterns should be explicit.
- The architecture should make it easy to understand which systems read or write which component sets.
- ECS should remain legible and not collapse into a generic “everything bag” architecture.

### Data-oriented design

- Prefer layouts and access patterns that support cache efficiency and predictable iteration.
- Keep hot-path data compact and easy to iterate.
- Separate hot runtime data from cold metadata, tooling data, and editor-facing structures where useful.
- Avoid mixing frequently updated simulation data with rarely used descriptive state.
- Favor batch processing, contiguous traversal, and explicit access patterns where appropriate.
- Design APIs and data structures so data flow is visible and cost is legible.
- Avoid object graphs or abstractions that destroy locality without strong justification.

### DAG-driven execution

- Prefer DAG-based modeling of engine tasks and render tasks where dependencies, ordering, and parallelism matter.
- Scheduling should be explicit, inspectable, and derivable from declared dependencies.
- Render work should be modeled through explicit dependency graphs rather than opaque sequencing.
- Engine task execution should prefer explicit dependency edges over ad hoc ordering conventions.
- DAGs should clarify execution and synchronization, not become ceremonial graph-building overhead.
- The dependency model should support future parallel execution, frame scheduling, and debugging.
- Systems that participate in scheduling should make read/write/resource dependencies visible.

### Reflection

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

### Patterns and abstractions

- Use patterns when they improve:
  - ECS clarity
  - subsystem separation
  - ownership clarity
  - scheduling clarity
  - facade stability
  - extension points
  - backend interchangeability
  - reflection integration
  - API safety
- Do not apply patterns ceremonially.
- Favor directness over abstraction layering.
- Avoid introducing wrappers, managers, helpers, or factories unless they solve a real architectural problem.
- Prefer zero-cost or near-zero-cost abstractions where practical.

### Ownership, lifetime, and mutability

- Ownership and lifetime rules must be explicit.
- The architecture should make it easy to answer:
  - who owns this data
  - who creates it
  - who mutates it
  - when it is valid
  - when it is destroyed
  - which phase or thread may access it
- Avoid hidden global mutable state.
- Avoid ambiguous ownership between ECS storage, resource systems, subsystem facades, and game-facing code.
- Prefer explicit handles, registries, arenas, pools, or resource managers where appropriate.
- Mutation rules should be visible and enforceable.

### Code quality

- Functions and methods should have one clear responsibility.
- Keep control flow understandable.
- Prefer simple, direct code over indirection-heavy abstractions.
- Avoid pass-through layers with no semantic value.
- Avoid speculative generalization.
- Use names that reflect engine concepts precisely.
- Optimize for readability, maintainability, and local reasoning.
- Be concise, but not clever.

---

## C++ language policy

- Prefer C++20/23 features when they clearly improve correctness, clarity, maintainability, or zero-cost abstraction and the toolchain supports them.
- Prefer STL first.
- Prefer `std::span`, `std::array`, `std::vector`, `std::string_view`, `std::optional`, `std::variant`, and concepts where appropriate.
- Prefer RAII and explicit ownership.
- Prefer `constexpr` and compile-time composition where practical.
- Avoid raw owning pointers. Prefer references to pointers.
- Avoid macro-heavy metaprogramming when language features suffice.
- Avoid custom containers, allocators, type-erasure systems, or utility frameworks unless profiling, layout requirements, or platform constraints justify them.
- Avoid virtual dispatch on hot paths unless clearly warranted.
- Prefer value semantics where they fit the design.
- Prefer explicit move/copy behavior over accidental ownership semantics.
- Prefer standard algorithms and ranges when they improve clarity without hiding important cost.
- Be careful with template metaprogramming: use it to encode intent and remove runtime cost, not to show cleverness.
- Prefer less code using templates to verbose code production. Use &&, auto, concepts when possible and appropriate.

---

## Architectural priorities for this repository

When making decisions, bias toward the following:

1. frame-time performance
2. reusable engine core across many projects
3. low technical debt and clean long-term architecture
4. explicit subsystem facades
5. slim and strongly typed APIs
6. compile-time enforcement where practical
7. data locality and predictable memory behavior
8. explicit DAG-based execution and dependency modeling
9. clear ECS boundaries
10. explicit ownership and lifecycle

---

## Required workflow before making changes

Before proposing or making changes, do the following:

1. Summarize the purpose of the relevant subsystem, module, or file.
2. Identify the main responsibilities and boundaries involved.
3. Describe how this code fits into the ECS and engine architecture.
4. Identify which facade or subsystem boundary it belongs behind.
5. Trace the main control-flow, data-flow, and dependency-flow paths.
6. Identify hot-path and cold-path concerns.
7. Identify ownership, lifetime, mutability, and compile-time/runtime responsibilities.
8. Identify any API surface affected by the code.
9. Evaluate the implementation against the standards in this file.
10. Separate findings into:
   - directly evidenced by code
   - inferred from structure or naming
   - speculative / uncertain
11. Propose a minimal, staged plan consistent with long-term reuse, performance, and low technical debt.
12. Only then propose or apply code changes.

Do not present a rewrite before first presenting an explanation and diagnosis unless explicitly asked for code only.

---

## How to review architecture

When asked to review architecture, always provide:

- subsystem purpose
- main responsibilities
- boundaries to other subsystems
- subsystem facade shape
- public API surface
- dependency structure
- execution flow
- data flow
- ownership and lifetime model
- hot-path vs cold-path separation
- compile-time vs runtime responsibility split
- extension points
- reflection integration points
- DAG/scheduling role if applicable
- strengths of the current design
- weaknesses of the current design
- risks to performance
- risks to reuse
- risks to API abuse
- risks to technical debt
- top refactoring or design priorities

When useful, explicitly distinguish between:

- intentional engine architecture
- ECS core design
- scheduling/graph design
- temporary scaffolding
- accidental complexity
- unclear or unverifiable design intent

---

## How to review ECS design

When reviewing ECS-related code, provide:

- what belongs to entities
- what belongs to components
- what belongs to systems
- whether behavior is placed in the right layer
- how data is laid out and accessed
- which systems read and write which component sets
- whether scheduling assumptions are explicit
- whether mutability rules are clear
- whether component access patterns are likely cache-friendly
- whether the design supports future system composition and parallelism
- whether compile-time constraints are used effectively
- whether the API reveals too much storage detail or too little execution detail

Call out especially:

- logic embedded in components
- entity types becoming pseudo-classes
- systems with too many responsibilities
- unclear read/write ownership
- excessive random access or scattered iteration
- abstractions that defeat data locality
- patterns that make scheduling or parallelism harder
- ECS APIs that are elegant on paper but expensive in practice

---

## How to review DAG/task systems

When reviewing engine task graphs, frame graphs, or render graphs, provide:

- what the nodes represent
- what the edges represent
- how dependencies are declared
- how execution order is derived
- where synchronization or barriers emerge
- whether the graph exposes enough information for debugging and profiling
- whether the graph supports incremental extension cleanly
- whether graph construction APIs are slim and hard to misuse
- whether the design supports future parallel execution and backend adaptation

Call out especially:

- hidden ordering assumptions
- implicit side effects not represented in the graph
- nodes that combine too many concerns
- graph APIs that allow invalid dependency states too easily
- graph designs that are generic but not actually useful
- runtime graph complexity that could be compile-time or precomputed

---

## How to review a subsystem

When reviewing a subsystem, provide:

- what the subsystem is responsible for
- what it should not be responsible for
- its facade and intended consumers
- its internal layers, if any
- its dependencies
- its lifecycle and initialization model
- how it interacts with ECS, reflection, and scheduling, if applicable
- how it exchanges data with the rest of the engine
- whether its abstractions are stable and reusable
- whether it leaks implementation details
- whether it is too rigid or too generic
- how easily it could support future engine requirements

Call out especially:

- unclear extension points
- subsystem leakage
- hidden coupling
- lifetime ambiguity
- fragile sequencing assumptions
- APIs that expose internal structure rather than engine concepts
- game-specific assumptions leaking into engine code
- debt that will become expensive later

---

## How to review a file

When asked to review a file, provide:

- file purpose
- major types / functions and their roles
- which subsystem it belongs to
- whether it is ECS core, runtime system, infrastructure, public API, implementation, or utility
- key inputs and outputs
- side effects
- ownership and lifetime assumptions
- hot-path vs cold-path relevance
- complexity sources
- duplication, verbosity, and unnecessary indirection
- whether the code is placed in the right architectural layer

Call out:

- fake abstractions
- weak or unstable APIs
- mixed levels of abstraction
- orchestration mixed with low-level mechanics
- engine-internal storage details leaking into outward-facing interfaces
- implicit lifecycle assumptions
- future extension pain points
- performance costs hidden behind clean-looking APIs

---

## How to explain code

When asked to explain code, use progressive zoom:

1. high-level summary
2. subsystem context
3. ECS context if relevant
4. logical chunk breakdown
5. block-by-block explanation
6. line-by-line explanation for nontrivial sections

For each chunk, explain:

- what it does
- why it exists
- which subsystem concern it belongs to
- what data it reads or writes
- what it owns or mutates
- whether it is likely on a hot path
- whether it affects API shape, reuse, or engine patterns
- whether it could be simpler, faster, or more extensible

Do not default to full line-by-line explanation for large files.
Start with architecture, ECS role, boundaries, and intent first.

---

## API design rules

APIs are a first-class concern in this repository.

When creating or reviewing APIs:

- prefer explicit names and explicit semantics
- make ownership, lifetime, mutability, validity, and cost clear
- expose engine concepts, not incidental storage details
- keep the API surface small but powerful
- encode correctness through types where practical
- prefer strong types to ambiguous primitives
- prefer compile-time restrictions where they materially improve safety
- avoid overloading one type or interface with many unrelated responsibilities
- separate runtime-facing API from tooling/editor concerns where useful
- design APIs so future extensions do not require breaking core contracts
- avoid god interfaces
- avoid APIs that force callers to know too much about internal sequencing
- avoid convenience methods that blur lifecycle boundaries
- make expensive work, synchronization, iteration, invalidation, and allocation visible where relevant

When evaluating an API, consider:

- is it easy to understand
- is it hard to misuse
- does it leak implementation details
- can it grow without breaking callers
- does it encode the right engine concepts
- are cost and side effects sufficiently visible
- does it support reuse across multiple games rather than one project shape

---

## Reuse rules

This engine should be built once and reused many times.

When proposing architecture or code changes, prefer designs that:

- solve engine-level problems rather than one-game-specific problems
- support multiple game genres and project structures
- allow project-specific code to live on top of the engine rather than inside it
- preserve stable core APIs and facades
- keep customization points intentional and minimal
- separate reusable engine core from game-layer code, sample code, and editor code
- avoid assumptions tied to one content pipeline or gameplay model unless explicitly intended

A design is better for reuse when:

- new projects can adopt it without rewriting engine internals
- game-specific behavior can be added via systems, data, scripting, or extension points
- core types remain engine-level rather than vague
- engine modules can evolve independently
- extension does not require copy-paste forks of core code

Do not confuse reuse with genericity theater.

---

## Extendability rules

When proposing architecture or refactors, optimize for future extension in realistic engine directions.

A design is more extendable when:

- subsystems are replaceable or augmentable at clear boundaries
- new components and systems can be added without rewriting core ECS semantics
- new resource types can be registered cleanly
- new backends can be added without modifying high-level systems
- core contracts are stable and narrow
- features compose without requiring central rewrites
- lifecycle hooks and extension points are intentional
- the system can support tooling and runtime separation cleanly
- parallel scheduling remains possible as the system grows

Do not confuse extendability with abstraction count.

More interfaces, wrappers, and base classes do not automatically improve extensibility.

Prefer extension models that are:

- explicit
- narrow
- discoverable
- composable
- easy to test
- hard to misuse
- performance-aware

---

## Patterns guidance

Patterns should support the engine architecture, not dominate it.

Use patterns when they materially improve:

- ECS clarity
- system composition
- ownership clarity
- lifecycle management
- scheduling clarity
- subsystem separation
- extension points
- API coherence
- testability
- backend interchangeability

Be cautious with:

- inheritance hierarchies
- deep abstract base class trees
- service locator patterns
- global registries without clear ownership rules
- generic event systems used as a substitute for architecture
- factories with no real policy variation
- manager/helper/util abstractions that collapse many responsibilities
- abstractions that introduce per-entity overhead on hot paths

Prefer:

- composition
- explicit interfaces
- typed handles
- explicit registries
- data-oriented boundaries
- system queries with legible read/write behavior
- scheduling boundaries that make execution and synchronization visible
- command buffers/queues where deferred execution is appropriate
- backend abstraction at well-chosen seams

If using a pattern, explain what architectural problem it solves and what runtime cost it introduces.

---

## Data, lifetime, and ownership rules

When designing or reviewing code, explicitly address:

- who creates entities, components, resources, and systems
- who owns them
- who can mutate them
- what invalidates them
- how they are destroyed or recycled
- whether access is immediate, deferred, handle-based, or query-based
- which phase or thread may access them
- whether references remain stable across structural changes

Avoid:

- hidden singleton state
- unclear ownership transfer
- implicit borrowing rules
- mutation from many unrelated systems
- APIs that return raw access without lifecycle guarantees
- accidental dependence on initialization order
- stale handles/references after structural mutation

Resource-heavy engine areas should prefer explicit lifecycle models.

---

## Dependency rules

Dependencies between engine subsystems must be intentional.

Prefer dependency direction that preserves layering, for example:

- platform supports higher-level systems
- ECS core supports gameplay-facing systems
- rendering backend supports renderer
- asset I/O supports resource systems
- editor/tooling depends on runtime systems, not vice versa
- game code depends on engine APIs, not engine internals

Avoid:

- circular subsystem dependencies
- editor concerns leaking into runtime core
- rendering details leaking into generic ECS APIs
- platform-specific details leaking into generic engine modules
- asset loading semantics spread across unrelated systems
- game-specific logic embedded in engine core

If a dependency is required, explain why that direction is correct.

---

## Refactoring rules

When proposing refactors:

- prefer minimal, behavior-preserving steps
- present changes as a short sequence of coherent phases or commits
- explain why each step comes before the next
- prefer consolidation, boundary clarification, API cleanup, strong typing, and data-flow cleanup over abstraction proliferation
- do not rewrite large areas at once unless explicitly requested
- do not introduce new layers unless they clarify a real subsystem boundary, facade, extension seam, or data-access rule
- keep migration cost in mind
- preserve architectural optionality where possible
- consider runtime cost, memory behavior, compile-time complexity, and reuse impact explicitly

When suggesting a new abstraction, justify it in terms of at least one of:

- clearer subsystem boundary
- better facade/API coherence
- stronger type safety
- safer ownership/lifecycle semantics
- cleaner ECS behavior placement
- better backend interchangeability
- cleaner feature extension
- improved testability without architectural distortion
- reduced coupling
- removal of proven duplication
- clearer hot-path/cold-path separation
- better dependency/DAG modeling

---

## Expectations for code generation

Before writing code, first determine:

- where the change belongs architecturally
- which subsystem should own the behavior
- whether the change belongs in ECS core, a subsystem facade, internal implementation, or game-layer code
- which existing API or pattern should absorb the change
- whether a new facade entry point or extension seam is actually required
- what ownership and lifecycle rules apply
- what future extensions this design should accommodate
- what runtime costs and compile-time costs this design introduces

Generated code should:

- fit the engine architecture where it is sound
- improve the architecture where it is clearly weak, but only with minimal disruption
- reinforce coherent ECS and subsystem boundaries
- reinforce facade-based extension at subsystem seams
- use explicit names tied to engine concepts
- use strong types where semantics matter
- prefer compile-time guarantees where practical
- make data flow and ownership readable
- avoid incidental complexity
- avoid short-term hacks that reduce reuse
- avoid hot-path overhead that is not clearly justified

Do not create by default:

- pass-through managers
- generic helper layers
- unnecessary wrappers
- brittle inheritance trees
- central god objects
- service locators
- APIs that expose backend-specific details at high levels
- abstractions justified only by hypothetical future use
- game-specific shortcuts in reusable engine core
- convenience APIs that hide expensive structural ECS operations
- facades that become giant dumping grounds

---

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

---

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

---

## Evidence and uncertainty

For every important claim, distinguish clearly between:

- directly evidenced by code
- inferred from structure or naming
- speculative

Do not invent architectural intent not supported by the code.

If context is missing, say which files, APIs, or subsystem boundaries would clarify the issue.

When multiple interpretations are possible, present the leading interpretations and explain why.

---

## Preferred response style

Prefer structured output over vague prose.

Good output forms include:

- subsystem summaries
- ECS design reviews
- API reviews
- dependency views
- ownership/lifetime summaries
- hot-path vs cold-path analysis
- extension-point analysis
- issue lists with severity and scope
- phased design or refactoring plans

When asked for explanation, diagnosis, or review, do not immediately dump rewritten code.

---

## Anti-patterns to avoid

Treat the following as warning signs by default:

- unclear ECS boundaries
- logic embedded in components
- entity types acting like inheritance-based objects
- systems with mixed unrelated responsibilities
- APIs that leak storage internals without reason
- central god objects controlling unrelated systems
- hidden global mutable state
- service locator style dependency access
- inheritance-heavy frameworks for extensibility
- pass-through manager layers
- backend-specific details leaking into high-level engine APIs
- resource ownership ambiguity
- initialization-order dependence
- large functions mixing orchestration and low-level mechanics
- generic event buses replacing clear architecture
- wrappers around wrappers
- premature abstraction
- verbosity that hides a simple engine concept
- random-access-heavy ECS operations on hot paths without justification
- per-entity polymorphic overhead in core simulation loops
- game-specific assumptions embedded in reusable engine code
- facades that expose everything
- weakly typed APIs where misuse is easy
- runtime checks that should be compile-time checks
- custom infrastructure replacing STL without evidence

---

## Preferred review sequence for nontrivial tasks

For nontrivial tasks, use this order:

1. explain current subsystem structure
2. identify ECS role, key boundaries, and data ownership
3. identify hot paths, performance risks, and reuse risks
4. identify API and extendability risks
5. identify conflicts with these principles
6. propose a minimal, coherent plan
7. implement the smallest useful step
8. summarize what changed and what remains

---

## Repository-specific notes

If repository-specific conventions exist, follow them unless they clearly conflict with the principles above.

When conventions are missing or inconsistent, prefer:

- frame-time-aware design
- data locality
- explicit ECS boundaries
- stable and clear APIs
- composition over inheritance
- explicit ownership and lifecycle
- narrow extension seams
- fewer, stronger abstractions over many weak ones
- direct engine concepts over vague technical wrappers
- reusable engine core over one-project convenience
