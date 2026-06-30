Read AGENTS.md and src/versions/simple/AGENTS.md

 Implement a default facade-level camera controller for ViennaVulkanEngine.

  Goal:
  Move the camera movement logic currently embedded in examples/game/game.cpp into a reusable default camera controller controlled by keyboard input.

  Required behavior:
  - Provide a default camera controller usable by applications through the facade API.
  - It should control a vve::Camera.
  - Keyboard controls:
    - W: move forward
    - S: move backward
    - A: strafe left
    - D: strafe right
    - Left arrow: yaw left
    - Right arrow: yaw right
    - Up arrow: pitch up or down consistently with the current game behavior
    - Down arrow: opposite pitch
    - Q: fly up
    - E: fly down
  - Clamp pitch to avoid view singularities, preserving the current example behavior.
  - Preserve existing movement feel unless there is a clear reason to adjust it.
  - The controller should update the render camera through the public facade, not through simple/v4/v5 internals.
  - The example game should use the new default controller instead of manually maintaining cameraEye, yaw, pitch, and movement code.

  Suggested API direction:
  - Prefer a small facade type such as vve::CameraController or vve::DefaultCameraController.
  - It may hold camera state: eye, yaw, pitch, move speed, turn speed, pitch clamp.
  - It should expose a small update/apply API that accepts facade input and updates/returns a vve::Camera.
  - Avoid introducing extra abstractions unless needed.
  - Keep all public types composed from facade-defined types.

  Implementation steps:
  1. Read the existing camera motion logic in examples/game/game.cpp.
  2. Inspect facade input APIs in Window/Input-related files.
  3. Add the reusable controller to an appropriate facade module/file.
  4. Wire any needed implementation-independent support only through facade APIs.
  5. Replace the example game’s custom camera motion block with the new controller.
  6. Add or update tests for the controller behavior, including Q/E vertical movement.
  7. Build or run the most relevant existing tests if feasible.

  Acceptance criteria:
  - examples/game/game.cpp no longer implements its own WASD/arrow camera controller logic.
  - The game still moves the camera with W/A/S/D and arrow keys.
  - Q moves/flys up and E moves/flys down.
  - The controller is reusable from user applications via the facade.
  - No concrete engine internals leak into example code.
  - Tests cover at least forward/back, strafe, yaw/pitch, and Q/E vertical movement.