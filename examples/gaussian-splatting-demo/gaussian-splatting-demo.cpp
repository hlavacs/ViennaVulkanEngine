/**
 * @file gaussian-splatting-demo.cpp
 * @brief Simple demonstration of gaussian splatting rendering in VVE
 */

#include <iostream>
#include "VHInclude.h"
#include "VEInclude.h"

/**
 * @brief Demo system that creates a simple scene with a gaussian splat object
 */
class GaussianSplattingDemo : public vve::System {
public:
    GaussianSplattingDemo(vve::Engine& engine) : vve::System("GaussianSplattingDemo", engine) {
        m_engine.RegisterCallbacks({
            {this, 1000, "LOAD_LEVEL", [this](Message& message) { return OnLoadLevel(message); }},
            {this, 10000, "UPDATE", [this](Message& message) { return OnUpdate(message); }}
        });
    }

    ~GaussianSplattingDemo() = default;

private:
    vecs::Handle m_cameraHandle;
    vecs::Handle m_cameraNodeHandle;
    float m_cameraSpeed = 2.0f;

    // FPS tracking
    int m_frameCount = 0;
    double m_fpsAccumulator = 0.0;
    double m_fpsTimer = 0.0;
    double m_fpsUpdateInterval = 1.0;  // Print FPS every 1 second

    void GetCamera() {
        if (m_cameraHandle.IsValid() == false) {
            auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin();
            m_cameraHandle = handle;
            m_cameraNodeHandle = parent;
        }
    }

    bool OnLoadLevel(Message message) {
        auto& msg = message.template GetData<vve::System::MsgLoadLevel>();
        std::cout << "Initializing Gaussian Splatting Demo scene..." << std::endl;

        // Create camera (using Insert like other demos)
        auto cameraHandle = m_registry.Insert(
            vve::Name{ "Camera" },
            vve::Camera{
                .m_aspect = 16.0f / 9.0f,
                .m_near = 0.1f,
                .m_far = 1000.0f,
                .m_fov = 45.0f
            },
            vve::Position{ glm::vec3(0.0f, 0.0f, 2.0f) },  // Closer to splat at origin
            vve::Rotation{ mat3_t{1.0f} },
            vve::Scale{ vec3_t{1.0f, 1.0f, 1.0f} },
            vve::LocalToParentMatrix{ mat4_t{1.0f} },
            vve::LocalToWorldMatrix{ mat4_t{glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f))} },
            vve::ParentHandle{}
        );

        // Create gaussian splat object
        auto gaussianHandle = m_registry.Insert(
            vve::Name{ "GaussianSplat" },
            vve::GaussianSplat{vve::GaussianSplatData{
                .plyPath = "assets/gaussian-splatting/Rose.ply",
                .transform = glm::mat4(1.0f),
                .isEnvironment = false,
                .emissiveIntensity = 1.0f
            }},
            vve::Position{ glm::vec3(0.0f, 0.0f, 0.0f) },
            vve::Rotation{ mat3_t{1.0f} },
            vve::Scale{ vec3_t{1.0f, 1.0f, 1.0f} }
        );

        // Send object creation message
        m_engine.SendMsg(vve::System::MsgObjectCreate{ vve::ObjectHandle(gaussianHandle), vve::ParentHandle{}, this });

        GetCamera();

        std::cout << "Scene created: Camera + Gaussian splat" << std::endl;
        std::cout << "Controls: WASD to move, Arrow keys to adjust position" << std::endl;

        return false;
    }

    bool OnUpdate(Message message) {
        auto& msg = message.template GetData<vve::System::MsgUpdate>();
        float dt = static_cast<float>(msg.m_dt);

        // FPS tracking
        m_frameCount++;
        m_fpsAccumulator += dt;
        m_fpsTimer += dt;

        if (m_fpsTimer >= m_fpsUpdateInterval) {
            double avgFPS = m_frameCount / m_fpsAccumulator;
            double avgFrameTime = (m_fpsAccumulator / m_frameCount) * 1000.0;  // in ms
            std::cout << "[FPS] " << static_cast<int>(avgFPS) << " fps"
                      << " (" << avgFrameTime << " ms/frame)"
                      << " - " << m_frameCount << " frames in " << m_fpsAccumulator << "s"
                      << std::endl;
            m_frameCount = 0;
            m_fpsAccumulator = 0.0;
            m_fpsTimer = 0.0;
        }

        GetCamera();
        if (!m_cameraNodeHandle.IsValid()) return false;

        // Get camera parent position (the node that moves)
        auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);

        // Handle keyboard input (WASD movement)
        const bool* keystate = SDL_GetKeyboardState(nullptr);

        vec3_t movement{0.0f};
        if (keystate[SDL_SCANCODE_W]) movement.z -= 1.0f;  // Forward
        if (keystate[SDL_SCANCODE_S]) movement.z += 1.0f;  // Backward
        if (keystate[SDL_SCANCODE_A]) movement.x -= 1.0f;  // Left
        if (keystate[SDL_SCANCODE_D]) movement.x += 1.0f;  // Right
        if (keystate[SDL_SCANCODE_SPACE]) movement.y += 1.0f;  // Up
        if (keystate[SDL_SCANCODE_LSHIFT]) movement.y -= 1.0f;  // Down

        if (glm::length(movement) > 0.0f) {
            pos() += glm::normalize(movement) * m_cameraSpeed * dt;
        }

        return false;
    }
};

int main() {
    std::cout << "=================================\n";
    std::cout << "Gaussian Splatting Demo\n";
    std::cout << "=================================\n\n";

    vve::Engine engine("Gaussian Splatting Demo", vve::RendererType::RENDERER_TYPE_GAUSSIAN);
    GaussianSplattingDemo demo{engine};
    engine.Run();

    return 0;
}
