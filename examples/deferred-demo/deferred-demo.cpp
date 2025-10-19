
#include <iostream>
#include <utility>
#include <format>
#include "VHInclude.h"
#include "VEInclude.h"
#include <random>

class DeferredDemo : public vve::System {

public:
	DeferredDemo(vve::Engine& engine) : vve::System("DeferredDemo", engine) {

		m_engine.RegisterCallbacks({
			{this,   1000, "LOAD_LEVEL", [this](Message& message) { return OnLoadLevel(message); } },
			{this,  10000, "UPDATE", [this](Message& message) { return OnUpdate(message); } },
			{this, -10000, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } }
			});
	};

	~DeferredDemo() {};

private:

	void GetCamera() {
		if (m_cameraHandle.IsValid() == false) {
			auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin();
			m_cameraHandle = handle;
			m_cameraNodeHandle = parent;
		};
	}

	bool OnLoadLevel(Message message) {
		auto& msg = message.template GetData<vve::System::MsgLoadLevel>();
		std::cout << "Loading level: " << msg.m_level << std::endl;
		std::string level = std::string("Level: ") + msg.m_level;

		// Removes lights from VESceneManager
		for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::PointLight&>()) {
			m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
		}
		for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::DirectionalLight&>()) {
			m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
		}
		for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::SpotLight&>()) {
			m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
		}

		// Starting Camera position
		GetCamera();
		m_registry.Get<vve::Rotation&>(m_cameraHandle)() = mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f / 2.0f, vec3_t{1.0f, 0.0f, 0.0f}) }; 
		m_registry.Get<vve::Position&>(m_cameraNodeHandle)().x += 7.46f;
		m_registry.Get<vve::Position&>(m_cameraNodeHandle)().y -= 4.2f;

		// -----------------  Light Mesh -----------------
		m_engine.SendMsg(MsgSceneLoad{ vve::Filename{"assets/standard/sphere.obj"} });

		// If there is a path to sponza and/or curtains, load it
		sponza_active = !sponza.empty();
		curtains_active = sponza_active && !curtains.empty();

		if (!sponza_active) {

			// ----------------- Load Plane -----------------
			m_engine.SendMsg(MsgSceneLoad{ vve::Filename{plane_obj}, aiProcess_FlipWindingOrder });

			auto m_handlePlane = m_registry.Insert(
				vve::Position{ {0.0f,0.0f,0.0f } },
				vve::Rotation{ mat3_t { glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f)) } },
				vve::Scale{ vec3_t{1000.0f,1000.0f,1000.0f} },
				vve::MeshName{ plane_mesh },
				vve::TextureName{ plane_txt },
				vve::UVScale{ { 1000.0f, 1000.0f } }
			);

			m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(m_handlePlane), vve::ParentHandle{}, this });

			// ----------------- Load Cube -----------------

			auto handleCube = m_registry.Insert(
				vve::Position{ { randFloat(-50.0f, 50.0f), randFloat(-50.0f, 50.0f), 0.5f } },
				vve::Rotation{ mat3_t{1.0f} },
				vve::Scale{ vec3_t{1.0f} });

			m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(handleCube), vve::ParentHandle{}, vve::Filename{cube_obj}, aiProcess_FlipWindingOrder });

			// ----------------- Load Cornell -----------------

			m_engine.SendMsg(MsgSceneLoad{ vve::Filename{cornell_obj}, aiProcess_PreTransformVertices });
			auto handleCornell = m_registry.Insert(
				vve::Position{ { 0.0f, 0.0f, -0.1f } },
				vve::Rotation{ mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f / 2.0f, vec3_t{1.0f, 0.0f, 0.0f}) } },
				vve::Scale{ vec3_t{1.0f} }
			);
			m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(handleCornell), vve::ParentHandle{}, vve::Filename{cornell_obj}, aiProcess_PreTransformVertices });

			// -----------------  Fireplace -----------------
			aiPostProcessSteps flags = static_cast<aiPostProcessSteps>(aiProcess_PreTransformVertices | aiProcess_ImproveCacheLocality);
			m_engine.SendMsg(MsgSceneLoad{ vve::Filename{fireplace}, flags });
			auto handleFireplace = m_registry.Insert(
				vve::Position{ { 5.0f, 0.0f, 0.1f } },
				vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))} },
				//vve::Rotation{ mat3_t{1.0f} },
				vve::Scale{ vec3_t{1.0f} }
			);
			m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(handleFireplace), vve::ParentHandle{}, vve::Filename{fireplace}, flags });

		}
		else {
			// -----------------  Sponza -----------------
			std::cout << "Sponza Scene courtesy of Intel Corporation - GPU Research Samples\n";

			// sponza
			aiPostProcessSteps flags = static_cast<aiPostProcessSteps>(aiProcess_PreTransformVertices | aiProcess_ImproveCacheLocality);
			m_engine.SendMsg(MsgSceneLoad{ vve::Filename{sponza}, flags });
			auto handleSponza = m_registry.Insert(
				vve::Position{ { 5.0f, 0.0f, 0.1f } },
				vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))} },
				vve::Scale{ vec3_t{1.0f} }
			);
			m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(handleSponza), vve::ParentHandle{}, vve::Filename{sponza}, flags });

			// curtains
			if (curtains_active) {
				m_engine.SendMsg(MsgSceneLoad{ vve::Filename{curtains}, flags });
				auto handleCurtains = m_registry.Insert(
					vve::Position{ { 5.0f, 0.0f, 0.1f } },
					vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))} },
					vve::Scale{ vec3_t{1.0f} }
				);
				m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(handleCurtains), vve::ParentHandle{}, vve::Filename{curtains}, flags });
			}
		}

		// -----------------  Moving point light on scene load -----------------
		manageMovingPointLight();

		return false;
	};

	bool OnUpdate(Message& message) {
		auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
		pos().z = 1.0f;
		if (!sponza_active) {
			pos().z = 1.5f;
		}
		
		// this moves the m_testHandle point light as long as point lights are not modified, this is just for light move testing
		if (m_registry.Exists(m_testHandle) && m_testHandle.IsValid()){
			auto pos = m_registry.Get<vve::Position&>(m_testHandle);
			static int sign = 1;
			if (pos().x > 10.0f || pos().x < 5.0f) sign *= -1;
			pos().x += 0.001f * sign;
			m_registry.Put<vve::Position&>(m_testHandle, pos);
		}

		return false;
	}

	bool OnRecordNextFrame(Message message) {
		// ImGui Interface
		ImGui::Begin("Light-Settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize| ImGuiWindowFlags_NoMove);

		// Point Light UI
		ImGui::Text("Select Point Light amount:");
		static constexpr int options[] = { 0, 1, 5, 10, 20, 40, 80 };
		static int activePointLights = 0;
		for (size_t idx = 0; idx < std::size(options); ++idx) {
			uint16_t val = options[idx];
			char buf[16];
			sprintf(buf, "%d", val);
			if (ImGui::Button(buf, ImVec2(40, 30))) {
				activePointLights = val;
				managePointLights(val);
			}
			if (idx + 1 < std::size(options))
				ImGui::SameLine();
		}

		ImGui::Text("Currently active Point Lights: %d", activePointLights);
		ImGui::Separator();

		// Spot Light UI
		ImGui::Text("Select moving Point Light setting:");
		static constexpr const char* pOptions[] = { "Off", "On" };
		static int activePIdx = 0;
		for (int i = 0; i < 2; ++i) {
			if (ImGui::Button(pOptions[i])) {
				activePIdx = i;
				managePointLights(0); // deletes all point lights
				if (i == 1) manageMovingPointLight();
			}
			ImGui::SameLine();
		}
		ImGui::NewLine();

		ImGui::Text("Active: %s", pOptions[activePIdx]);

		ImGui::Separator();

		// Spot Light UI
		ImGui::Text("Select Spot Light setting:");
		static constexpr const char* spotOptions[] = { "None", "Couch" };
		static int activeSpotIdx = 0;
		for (int i = 0; i < 2; ++i) {
			if (ImGui::Button(spotOptions[i])) {
				activeSpotIdx = i;
				manageSpotLights(activeSpotIdx);
			}
			ImGui::SameLine();
		}
		ImGui::NewLine();

		ImGui::Text("Active: %s", spotOptions[activeSpotIdx]);

		ImGui::Separator();

		// Shadow On/Off toggle UI
		ImGui::Checkbox("Enable Shadow", &m_engine.GetShadowToggle());
		if (m_engine.IsShadowEnabled()) {
			ImGui::Text("Shadow ON");
		}
		else {
			ImGui::Text("Shadow OFF");
		}

		ImGui::End();

		return false;
	}

	void manageMovingPointLight() {
		// -----------------  Moving point light on scene load -----------------

		glm::vec3 pointLightPosition(5.0f, 1.8f, 2.5f);
		glm::vec3 pointLightColor(0.97f, 0.71f, 0.184f);
		float intensity = 0.6f;

		vvh::Color sphereColor{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
		m_testHandle = m_registry.Insert(
			vve::Name{ "PointLight-test" },
			vve::PointLight{ vvh::LightParams{
				.color = pointLightColor,
				.params = glm::vec4(1.0f, intensity, 10.0f, 0.01f),
				.attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
			} },
			vve::Position{ pointLightPosition },
			vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), glm::radians(-70.0f), glm::vec3(1.0f, 0.0f, 0.0f))} },
			vve::Scale{ vec3_t{0.01f, 0.01f, 0.01f} },
			vve::LocalToParentMatrix{ mat4_t{1.0f} },
			vve::LocalToWorldMatrix{ mat4_t{1.0f} },
			sphereColor,
			vve::MeshName{ "assets/standard/sphere.obj/sphere" }
			);
		m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(m_testHandle), vve::ParentHandle{}, this });
	}

	void managePointLights(const uint16_t& lightCount) {
		// deletes all lights first
		for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::PointLight&>()) {
			m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
		}
		static constexpr vvh::Color sphereColor{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

		for (uint16_t i = 0; i < lightCount; ++i) {
			// Random values but still close to main scene
			glm::vec3 pointLightPosition(randFloat(5.0f, 10.0f), randFloat(0.5f, 3.0), randFloat(1.75f, 2.5f));
			if (sponza_active) {
				pointLightPosition = glm::vec3(randFloat(-10.0f, 10.0f), randFloat(-5.5f, 5.5), randFloat(0.75f, 15.5f));
			}
			glm::vec3 pointLightColor(randFloat(0.0f, 1.0f), randFloat(0.0f, 1.0f), randFloat(0.0f, 1.0f));
			float intensity = randFloat(0.1f, 0.3f);
			intensity *= sponza_active ? 10 : 1;


			auto lightHandle = m_registry.Insert(
				vve::Name{ "PointLight-" + i },
				vve::PointLight{ vvh::LightParams{
					.color = pointLightColor,
					.params = glm::vec4(1.0f, intensity, 10.0f, 0.01f),
					.attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
				} },
				vve::Position{ pointLightPosition },
				vve::Rotation{ mat3_t{1.0f} },
				vve::Scale{ vec3_t{0.01f, 0.01f, 0.01f} },
				vve::LocalToParentMatrix{ mat4_t{1.0f} },
				vve::LocalToWorldMatrix{ mat4_t{1.0f} },
				sphereColor,
				vve::MeshName{ "assets/standard/sphere.obj/sphere" }
				);
			m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle), vve::ParentHandle{}, this });
		}
	}

	void manageSpotLights(const int spotOption) {
		// deletes all lights first
		for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::SpotLight&>()) {
			m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle });
		}
		static constexpr vvh::Color sphereColor{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

		if (spotOption == 0) return;

		vvh::Color color3{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.1f, 0.1f, 0.9f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
		float intensity3 = 2.9f;
		auto lightHandle3 = m_registry.Insert(
			vve::Name{ "SpotLight-1" },
			vve::SpotLight{ vvh::LightParams{
				.color = glm::vec3(1.0f, 1.0f, 1.0f),
				.params = glm::vec4(3.0f, intensity3, 10.0, 0.01f),
				.attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
			} },
			vve::Position{ glm::vec3(7.0f, 1.5f, 2.0f) },
			vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), -3.14152f / 5.0f, glm::vec3(1.0f,0.0f,0.0f)) } },
			vve::Scale{ vec3_t{0.01f, 0.05f, 0.01f} },
			vve::LocalToParentMatrix{ mat4_t{1.0f} },
			vve::LocalToWorldMatrix{ mat4_t{1.0f} },
			color3,
			vve::MeshName{ "assets/standard/sphere.obj/sphere" }
			);
		m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle3), vve::ParentHandle{}, this });
	}

	// Random generator functions
	static std::mt19937& getRng() {
		static std::mt19937 rndEngine{ std::random_device{}() };
		return rndEngine;
	}

	float randFloat(float min, float max) {
		std::uniform_real_distribution<float> dist(min, max);
		return dist(getRng());
	}

private:
	vecs::Handle m_cameraHandle{};
	vecs::Handle m_cameraNodeHandle{};
	vecs::Handle m_testHandle{};

	inline static const std::string plane_obj	{ "assets/test/plane/plane_t_n_s.obj" };
	inline static const std::string plane_mesh	{ "assets/test/plane/plane_t_n_s.obj/plane" };
	inline static const std::string plane_txt	{ "assets/test/plane/grass.jpg" };
	inline static const std::string cube_obj	{ "assets/test/crate0/cube.obj" };
	inline static const std::string cornell_obj	{ "assets/test/cornell/CornellBox-Original.obj" };
	inline static const std::string fireplace	{ "assets/test/Fireplace/Fireplace.gltf" };
	// Sponza Scene courtesy of Intel Corporation - GPU Research Samples
	bool sponza_active = false;
	bool curtains_active = false;
	// If there is a path, it loads this instead of the Fireplace demo
	inline static const std::string sponza		{ /*"INSERT-YOUR-PATH/main_sponza/NewSponza_Main_glTF_003.gltf"*/ };
	inline static const std::string curtains	{ /*"INSERT-YOUR-PATH/pkg_a_curtains/NewSponza_Curtains_glTF.gltf"*/ };
};



int main() {
	vve::Engine engine("My Engine");
	DeferredDemo demo{ engine };
	engine.Run();

	return 0;
}

