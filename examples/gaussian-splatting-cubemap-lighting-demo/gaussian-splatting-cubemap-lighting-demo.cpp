// ============================================================================
// DISCLAIMER:
// The implementation has been adapted with the assistance of generative AI.
// ============================================================================

/**
 * @file gaussian-splatting-cubemap-lighting-demo.cpp
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
    glm::vec3 gaussianPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 gaussianRotation = glm::vec3(0.0f, 0.0f, 0.0f);  // Euler angles (degrees)
    glm::vec3 gaussianScale = glm::vec3(1.0f, 1.0f, 1.0f);

    // Mesh transform (initial)
    glm::vec3 meshPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 meshScale = glm::vec3(0.03f, 0.03f, 0.03f);
    //glm::vec3 meshScale = glm::vec3(1.0f, 1.0f, 1.0f);

    // Lighting
    // Ambient intensity multiplier in shader (edit Common_Light.slang)

    glm::vec3 cameraPositionOffset = glm::vec3(0.0f, -1.5f, -3.0f);

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
            {this, 10000, "UPDATE", [this](Message& message) { return OnUpdate(message); }}
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
    glm::mat3 m_gaussianInitialRotation = glm::mat3(1.0f);  // Set in OnLoadLevel
    glm::vec3 m_gaussianPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_gaussianRotation = glm::vec3(-85.0f, 0.0f, -100.0f);  // Runtime delta (starts at 0)
    float m_transformSpeed = 1.0f;

    // IBL regeneration timing
    int m_extractionFrameCounter = 0;

    void GetCamera() {
        if (m_cameraHandle.IsValid() == false) {
            auto view = m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>();
            if (view.begin() != view.end()) {
                auto [handle, camera, parent] = *view.begin();
                m_cameraHandle = handle;
                m_cameraNodeHandle = parent;
            }
        }
    }

    static glm::mat3 EulerToRotationMatrix(const glm::vec3& eulerDegrees) {
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(eulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(eulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), glm::radians(eulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        return glm::mat3(rotZ * rotY * rotX);
    }

    bool OnLoadLevel(Message message) {
        auto& msg = message.template GetData<vve::System::MsgLoadLevel>();
        std::cout << "Initializing Hybrid Lighting Demo scene..." << std::endl;

        // OPTIONAL: Remove VVE's default scene lights (created by VESceneManager::OnLoadLevel)
        for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::PointLight&>()) {
            m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        }
        for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::DirectionalLight&>()) {
            m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        }
        for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::SpotLight&>()) {
            m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
        }

        // Build initial rotation: config (baked) * member initial value (runtime delta)
        m_gaussianInitialRotation = EulerToRotationMatrix(m_config.gaussianRotation);
        glm::mat3 startRotation = m_gaussianInitialRotation * EulerToRotationMatrix(m_gaussianRotation);

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
            vve::Rotation{ startRotation },
            vve::Scale{ m_config.gaussianScale }
        );

        // Store initial position for runtime adjustment
        m_gaussianPosition = m_config.gaussianPosition;

        // Send gaussian object creation message
        m_engine.SendMsg(vve::System::MsgObjectCreate{ vve::ObjectHandle(m_gaussianHandle), vve::ParentHandle{}, this });

        // Load mesh (using config)
        m_engine.SendMsg(vve::System::MsgSceneLoad{ vve::Filename{m_config.meshPath} });

        // Create mesh object instance (using config)
        m_meshObjectHandle = m_registry.Insert(
            vve::Name{ "TestMesh" },
            vve::Position{ m_config.meshPosition },
            vve::Rotation{ mat3_t{ glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(.0f, 0.0f, 1.0f)) } },
            vve::Scale{ m_config.meshScale }
        );

        m_engine.SendMsg(vve::System::MsgSceneCreate{ vve::ObjectHandle(m_meshObjectHandle), vve::ParentHandle{}, vve::Filename{m_config.meshPath} });

        // Set initial camera rotation and position (following deferred-demo convention)
        GetCamera();
        if (m_cameraHandle.IsValid()) {
            // Set intial camera rotation
            m_registry.Get<vve::Rotation&>(m_cameraHandle)() =
                mat3_t{ glm::rotate(mat4_t{1.0f}, glm::radians(90.0f), vec3_t{1.0f, 0.0f, 0.0f}) };

            // Apply position offset
            m_registry.Get<vve::Position&>(m_cameraNodeHandle)() += m_config.cameraPositionOffset;
        }


        std::cout << "Scene created." << std::endl;
        std::cout << "\nNote: Small sphere in scene is light visualization from VESceneManager (shows cubemap if bound)" << std::endl;
        std::cout << "\nControls (VVE camera - VEGUI.cpp):" << std::endl;
        std::cout << "  WASD - Move camera (relative to view direction)" << std::endl;
        std::cout << "  Q/E - Move camera up/down (world Z)" << std::endl;
        std::cout << "  Arrow Keys / Mouse+RMB - Rotate camera" << std::endl;
        std::cout << "  Shift - Speed multiplier" << std::endl;
        std::cout << "\nGaussian Transform Controls (Numpad):" << std::endl;
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
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << avgFPS
                      << " (" << avgFrameTime << " ms/frame)" << std::endl;

            // Periodic GPU timing output (for automated benchmarking)
            if (m_gaussianRenderer) {
                const auto& t = m_gaussianRenderer->GetFrameTimings();
                if (t.valid) {
                    double toMs = t.timestampPeriod / 1e6;
                    std::cout << "Rank: " << std::fixed << std::setprecision(3) << t.rankTime * toMs << " ms, "
                              << "Sort: " << t.sortTime * toMs << " ms, "
                              << "Projection: " << t.projectionTime * toMs << " ms, "
                              << "Render: " << t.renderTime * toMs << " ms" << std::endl;
                }
                const auto& ibl = m_gaussianRenderer->GetIBLTimings();
                if (ibl.valid) {
                    double toMs = ibl.timestampPeriod / 1e6;
                    std::cout << "Cubemap: " << std::fixed << std::setprecision(3) << ibl.cubemapTime * toMs << " ms, "
                              << "Irradiance: " << ibl.irradianceTime * toMs << " ms" << std::endl;
                }
            }

            m_frameCount = 0;
            m_fpsAccumulator = 0.0;
            m_fpsTimer = 0.0;
        }

        GetCamera();
        if (!m_cameraNodeHandle.IsValid()) return false;

        // Camera movement handled by VVE's VEGUI (WASD, Q/E, arrows, mouse)
        const bool* keystate = SDL_GetKeyboardState(nullptr);

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
            m_registry.Get<vve::Position&>(m_gaussianHandle)() = m_gaussianPosition;

            // Final rotation = initial (baked) * runtime (delta)
            glm::mat3 runtimeRotation = EulerToRotationMatrix(m_gaussianRotation);
            m_registry.Get<vve::Rotation&>(m_gaussianHandle)() = m_gaussianInitialRotation * runtimeRotation;

            std::cout << "Gaussian: pos(" << m_gaussianPosition.x << ", " << m_gaussianPosition.y << ", " << m_gaussianPosition.z << ") "
                      << "rot delta(" << m_gaussianRotation.x << ", " << m_gaussianRotation.y << ", " << m_gaussianRotation.z << ")" << std::endl;
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
