/**
 * @file hybrid-lighting-demo.cpp
 * @brief Hybrid rendering: Mesh object lit by gaussian environment
 *
 * Light probe extraction from gaussian splatting environments
 * Demonstrates:
 * - Gaussian environment rendering
 * - Traditional mesh rendering
 * - Dynamic IBL extraction from gaussian environment (cubemap method)
 */

#include <iostream>
#include "VHInclude.h"
#include "VEInclude.h"


struct SceneConfig {
    // Asset paths
    const char* gaussianPath = "assets/gaussian-splatting/InteriorDesign.ply";
    const char* meshPath = "assets/standard/sphere.obj";
    //const char* meshPath = "assets/viking_room/viking_room.obj";

    // Gaussian transform (initial)
    glm::vec3 gaussianPosition = glm::vec3(-2.0f, 0.0f, 0.0f);
    glm::vec3 gaussianRotation = glm::vec3(0.0f, 80.0f, -140.0f);  // Euler angles (degrees)
    glm::vec3 gaussianScale = glm::vec3(1.0f, 1.0f, 1.0f);

    // Mesh transform (initial)
    glm::vec3 meshPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 meshScale = glm::vec3(0.03f, 0.03f, 0.03f);
    //glm::vec3 meshScale = glm::vec3(1.0f, 1.0f, 1.0f);

    // Lighting
    // Ambient multiplier in shader (edit Common_Light.slang)
    float lightIntensity = 0.2f;         // Directional light intensity
    glm::vec3 lightColor = glm::vec3(0.3f, 0.3f, 0.3f);

    // Camera
    glm::vec3 cameraPosition = glm::vec3(0.0f, 1.0f, 5.0f);
    float cameraFov = 45.0f;
    float cameraSpeed = 2.0f;

    // Rendering
    glm::vec3 clearColor = glm::vec3(0.1f, 0.1f, 0.1f);  // Main scene background (dark grey)
    glm::vec3 cubemapClearColor = glm::vec3(0.3f, 0.3f, 0.3f);  // Cubemap background (neutral grey for ambient fallback)

    // IBL generation
    int iblUpdateInterval = 1;  // Regenerate cubemap every N frames (1 = every frame for dynamic lighting)
};

/**
 * @brief Hybrid scene demo: Traditional mesh object inside gaussian splatting environment
 */
class HybridLightingDemo : public vve::System {
public:
    HybridLightingDemo(vve::Engine& engine) : vve::System("HybridLightingDemo", engine) {
        m_engine.RegisterCallbacks({
            {this, 1000, "LOAD_LEVEL", [this](Message& message) { return OnLoadLevel(message); }},
            {this, 1000, "UPDATE", [this](Message& message) { return OnUpdate(message); }}
        });
    }

    ~HybridLightingDemo() = default;

private:
    SceneConfig m_config;

    vecs::Handle m_cameraHandle;
    vecs::Handle m_cameraNodeHandle;
    vecs::Handle m_meshObjectHandle;
    vecs::Handle m_gaussianHandle;
    vve::RendererGaussian* m_gaussianRenderer = nullptr;

    // FPS tracking
    int m_frameCount = 0;
    double m_fpsAccumulator = 0.0;
    double m_fpsTimer = 0.0;
    double m_fpsUpdateInterval = 1.0;

    // Gaussian transform adjustment
    glm::vec3 m_gaussianPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_gaussianRotation = glm::vec3(0.0f, 0.0f, 0.0f);
    float m_transformSpeed = 1.0f;

    // IBL regeneration timing
    int m_extractionFrameCounter = 0;

    void GetCamera() {
        if (m_cameraHandle.IsValid() == false) {
            auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin();
            m_cameraHandle = handle;
            m_cameraNodeHandle = parent;
        }
    }

    bool OnLoadLevel(Message message) {
        auto& msg = message.template GetData<vve::System::MsgLoadLevel>();
        std::cout << "Initializing Hybrid Lighting Demo scene..." << std::endl;

        // OPTIONAL: Remove VVE's default scene lights (created by VESceneManager::OnLoadLevel)
        // VVE automatically creates 3 test lights: Light1 (point, red), Light2 (directional, green), Light3 (spot, blue)
        // Uncomment to use ONLY gaussian IBL for ambient lighting (no direct lights)
        // Currently KEPT for baseline visibility - gaussian IBL provides ambient, these provide directionality
        //
        // for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::PointLight&>()) {
        //     m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        // }
        // for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::DirectionalLight&>()) {
        //     m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        // }
        // for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::SpotLight&>()) {
        //     m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        // }

        // Create camera (using config)
        auto cameraHandle = m_registry.Insert(
            vve::Name{ "Camera" },
            vve::Camera{
                .m_aspect = 16.0f / 9.0f,
                .m_near = 0.1f,
                .m_far = 1000.0f,
                .m_fov = m_config.cameraFov
            },
            vve::Position{ m_config.cameraPosition },
            vve::Rotation{ mat3_t{1.0f} },
            vve::Scale{ vec3_t{1.0f, 1.0f, 1.0f} },
            vve::LocalToParentMatrix{ mat4_t{1.0f} },
            vve::LocalToWorldMatrix{ mat4_t{glm::translate(glm::mat4(1.0f), m_config.cameraPosition)} },
            vve::ParentHandle{}
        );

        // Build initial rotation matrix from config Euler angles (XYZ order)
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(m_config.gaussianRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(m_config.gaussianRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), glm::radians(m_config.gaussianRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat3 initialRotation = glm::mat3(rotY * rotX * rotZ);

        // Create gaussian environment (using config)
        m_gaussianHandle = m_registry.Insert(
            vve::Name{ "GaussianEnvironment" },
            vve::GaussianSplat{vve::GaussianSplatData{
                .plyPath = m_config.gaussianPath,
                .transform = glm::mat4(1.0f),
                .isEnvironment = true,
                .emissiveIntensity = 1.0f
            }},
            vve::Position{ m_config.gaussianPosition },
            vve::Rotation{ initialRotation },
            vve::Scale{ m_config.gaussianScale },
            vve::LocalToParentMatrix{ mat4_t{1.0f} },
            vve::LocalToWorldMatrix{ mat4_t{glm::translate(glm::mat4(1.0f), m_config.gaussianPosition)} }
        );

        // Store initial transform for runtime adjustment
        m_gaussianPosition = m_config.gaussianPosition;
        m_gaussianRotation = m_config.gaussianRotation;

        // Send gaussian object creation message
        m_engine.SendMsg(vve::System::MsgObjectCreate{ vve::ObjectHandle(m_gaussianHandle), vve::ParentHandle{}, this });

        // Load mesh (using config)
        m_engine.SendMsg(vve::System::MsgSceneLoad{ vve::Filename{m_config.meshPath} });

        // Create mesh object instance (using config)
        m_meshObjectHandle = m_registry.Insert(
            vve::Name{ "TestMesh" },
            vve::Position{ m_config.meshPosition },
            vve::Rotation{ mat3_t{1.0f} },
            vve::Scale{ m_config.meshScale }
        );

        m_engine.SendMsg(vve::System::MsgSceneCreate{ vve::ObjectHandle(m_meshObjectHandle), vve::ParentHandle{}, vve::Filename{m_config.meshPath} });

        // Add weak directional light for baseline visibility (using config)
        auto lightHandle = m_engine.CreateDirectionalLight(
            vve::Name{"WeakFillLight"}, vve::ParentHandle{},
            vve::DirectionalLight{vvh::LightParams{
                m_config.lightColor,
                glm::vec4(0.0f, m_config.lightIntensity, 10.0, 0.1f),
                glm::vec3(1.0f, 0.01f, 0.005f)
            }},
            vve::Position{glm::vec3(0.0f, 10.0f, 0.0f)},
            vve::Rotation{mat3_t{glm::rotate(glm::mat4(1.0f), -3.14152f / 4.0f, glm::vec3(1.0f, 0.0f, 0.0f))}}
        );

        GetCamera();

        std::cout << "Scene created." << std::endl;
        std::cout << "\nNote: Small sphere in scene is light visualization from VESceneManager (shows cubemap if bound)" << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  WASD + Space/Shift - Move camera" << std::endl;
        std::cout << "  Mouse/Arrow Keys - Rotate camera" << std::endl;
        std::cout << "  Numpad 4/6/8/2 - Move gaussian (X/Z)" << std::endl;
        std::cout << "  Numpad +/- - Move gaussian (Y)" << std::endl;
        std::cout << "  Numpad 7/9 - Rotate gaussian (Y axis)" << std::endl;
        std::cout << "  Numpad 1/3 - Rotate gaussian (X axis)" << std::endl;
        std::cout << "  Numpad / * - Rotate gaussian (Z axis)" << std::endl;

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
            double avgFrameTime = (m_fpsAccumulator / m_frameCount) * 1000.0;
            std::cout << "[FPS] " << static_cast<int>(avgFPS) << " fps"
                      << " (" << avgFrameTime << " ms/frame)"
                      << std::endl;
            m_frameCount = 0;
            m_fpsAccumulator = 0.0;
            m_fpsTimer = 0.0;
        }

        GetCamera();
        if (!m_cameraNodeHandle.IsValid()) return false;

        // Camera movement
        auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
        const bool* keystate = SDL_GetKeyboardState(nullptr);

        vec3_t movement{0.0f};
        if (keystate[SDL_SCANCODE_W]) movement.z -= 1.0f;  // Forward
        if (keystate[SDL_SCANCODE_S]) movement.z += 1.0f;  // Backward
        if (keystate[SDL_SCANCODE_A]) movement.x -= 1.0f;  // Left
        if (keystate[SDL_SCANCODE_D]) movement.x += 1.0f;  // Right
        if (keystate[SDL_SCANCODE_SPACE]) movement.y += 1.0f;  // Up
        if (keystate[SDL_SCANCODE_LSHIFT]) movement.y -= 1.0f;  // Down

        if (glm::length(movement) > 0.0f) {
            pos() += glm::normalize(movement) * m_config.cameraSpeed * dt;
        }

        // Gaussian transform adjustment controls (all on numpad to avoid conflicts)
        bool gaussianTransformChanged = false;

        // Position controls (numpad)
        if (keystate[SDL_SCANCODE_KP_4]) { m_gaussianPosition.x -= m_transformSpeed * dt; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_6]) { m_gaussianPosition.x += m_transformSpeed * dt; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_2]) { m_gaussianPosition.z += m_transformSpeed * dt; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_8]) { m_gaussianPosition.z -= m_transformSpeed * dt; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_PLUS]) { m_gaussianPosition.y += m_transformSpeed * dt; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_MINUS]) { m_gaussianPosition.y -= m_transformSpeed * dt; gaussianTransformChanged = true; }

        // Rotation controls (degrees per second)
        float rotSpeed = 45.0f * dt;
        if (keystate[SDL_SCANCODE_KP_7]) { m_gaussianRotation.y -= rotSpeed; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_9]) { m_gaussianRotation.y += rotSpeed; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_1]) { m_gaussianRotation.x -= rotSpeed; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_3]) { m_gaussianRotation.x += rotSpeed; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_DIVIDE]) { m_gaussianRotation.z -= rotSpeed; gaussianTransformChanged = true; }
        if (keystate[SDL_SCANCODE_KP_MULTIPLY]) { m_gaussianRotation.z += rotSpeed; gaussianTransformChanged = true; }

        // Update gaussian transform if changed
        if (gaussianTransformChanged && m_gaussianHandle.IsValid()) {
            auto gaussianPos = m_registry.Get<vve::Position&>(m_gaussianHandle);
            gaussianPos() = m_gaussianPosition;

            // Build rotation matrix from Euler angles (XYZ order)
            glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(m_gaussianRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(m_gaussianRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), glm::radians(m_gaussianRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat3 rotation = glm::mat3(rotY * rotX * rotZ);

            auto gaussianRot = m_registry.Get<vve::Rotation&>(m_gaussianHandle);
            gaussianRot() = rotation;

            std::cout << "Gaussian: pos(" << m_gaussianPosition.x << ", " << m_gaussianPosition.y << ", " << m_gaussianPosition.z << ") "
                      << "rot(" << m_gaussianRotation.x << ", " << m_gaussianRotation.y << ", " << m_gaussianRotation.z << ")" << std::endl;
        }

        // Get RendererGaussian reference on first frame (after all systems initialized)
        if (!m_gaussianRenderer) {
            // RendererDeferred creates RendererGaussian with name: "VVE Renderer Deferred" + "Gaussian"
            std::string gaussianSystemName = m_engine.m_rendererDeferredName + "Gaussian";

            auto* system = m_engine.GetSystem(gaussianSystemName);
            if (system) {
                m_gaussianRenderer = dynamic_cast<vve::RendererGaussian*>(system);

                if (m_gaussianRenderer) {
                    std::cout << "[IBL] RendererGaussian found, configuring and generating initial cubemap..." << std::endl;

                    // Configure cubemap clear color (neutral grey for ambient fallback)
                    m_gaussianRenderer->SetCubemapClearColor(m_config.cubemapClearColor);

                    // Generate IBL immediately on startup (don't wait 60 frames)
                    m_gaussianRenderer->GenerateCubemapIBL(0);
                    std::cout << "[IBL] Initial cubemap generated" << std::endl;
                } else {
                    std::cout << "[Error] System found but cast to RendererGaussian failed" << std::endl;
                }
            } else {
                std::cout << "[Error] RendererGaussian system not found at: " << gaussianSystemName << std::endl;
            }
        }

        // Regenerate cubemap IBL periodically (using config update interval)
        m_extractionFrameCounter++;
        if (m_gaussianRenderer && m_extractionFrameCounter >= m_config.iblUpdateInterval) {
            m_extractionFrameCounter = 0;
            m_gaussianRenderer->GenerateCubemapIBL(0);
        }

        return false;
    }
};

int main() {
    std::cout << "=================================\n";
    std::cout << "Hybrid Lighting Demo\n";
    std::cout << "Ambient Lighting from Gaussian Environment Cubemap\n";
    std::cout << "=================================\n\n";

    vve::Engine engine("Hybrid Lighting Demo", vve::RendererType::RENDERER_TYPE_DEFERRED);
    HybridLightingDemo demo{engine};
    engine.Run();

    return 0;
}
