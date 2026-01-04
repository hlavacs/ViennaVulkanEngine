#pragma once

namespace vve {
	class LightManager : public System {
	private:
		std::vector<LightSource> lights;
		DeviceBuffer<LightSource>* buffer;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		float totalLightWeight = 0.0f;

		bool lightChanged = false;

	public:

		LightManager(std::string systemName, Engine& engine, CommandManager* commandManager, VkDevice device, VkPhysicalDevice physicalDevice) : 
			System{ systemName, engine }, commandManager(commandManager), device(device), physicalDevice(physicalDevice) 
		{
			engine.RegisterCallbacks({
			{this,  1996, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			{this,  0000, "LIGHT_CREATE", [this](Message& message) { return OnLightCreate(message); } },
			{this,  1996, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } }
				});

			createBuffer();
		}

		bool OnLightCreate(Message message) {
			lightChanged = true;
			return false;
		}

		bool OnPrepareNextFrame(Message message) {
			if (lightChanged) {

				lights = std::vector<LightSource>();
				totalLightWeight = 0.0f;

				for (auto [oHandle, name, light, LtoW] : m_registry.GetView<vecs::Handle, Name, vvh::VRTSphereLight, LocalToWorldMatrix&>()) {
					glm::vec4 position = glm::vec4{ LtoW()[3] };

					addSphereLight(position, glm::vec4(light.emission, 1.0), light.radius);
				}

				for (auto [oHandle, name, light, LtoW] : m_registry.GetView<vecs::Handle, Name, vvh::VRTDiskLight, LocalToWorldMatrix&>()) {
					glm::vec4 position = glm::vec4{ LtoW()[3] };

					glm::vec4 transformedDirection = glm::mat4{ LtoW } * glm::vec4(light.direction, 0.0f);

					addDiskLight(position, glm::vec4(light.emission, 1.0), transformedDirection, light.radius);
				}

				for (LightSource& light : lights) {
					light.accumulativeSampleWeight /= totalLightWeight;
				}

				buffer->updateBuffer(lights.data(), lights.size());
				return false;
			}
			
		}

		bool OnRecordNextFrame(Message message) {
			lightChanged = false;
			return false;
		}
		
		
		void addSphereLight(glm::vec4 position, glm::vec4 emission, float radius) {
			LightSource light;
			float surfaceArea = calculateSphereLightSurfaceArea(radius);
			float sampleWeight = surfaceArea * getLightPower(emission);
			totalLightWeight += sampleWeight;


			light.position = position;
			light.emission = emission;
			light.radius = radius;
			light.lightType = 0;
			light.direction = glm::vec4(0.0, 0.0, 0.0, 0.0);
			light.pdf = surfaceArea;
			light.accumulativeSampleWeight = totalLightWeight;
			lights.push_back(light);
		}

		void addDiskLight(glm::vec4 position, glm::vec4 emission, glm::vec4 direction, float radius) {
			LightSource light;
			float surfaceArea = calculateDiskLightSurfaceArea(radius);
			float sampleWeight = surfaceArea * getLightPower(emission);
			totalLightWeight += sampleWeight;

			light.position = position;
			light.emission = emission;
			light.radius = radius;
			light.lightType = 1;
			light.direction = direction;
			light.pdf = surfaceArea;
			light.accumulativeSampleWeight = totalLightWeight;
			lights.push_back(light);
		}

		float getLightPower(glm::vec4 emission) {
			return emission.x + emission.y + emission.z;
		}

		//replace pi here
		float calculateSphereLightSurfaceArea(float radius) {
			return 4.0f * 3.14 * radius * radius;
		}

		float calculateDiskLightSurfaceArea(float radius) {
			return 3.14 * radius * radius;
		}

		void createBuffer() {
			buffer = new DeviceBuffer<LightSource>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, commandManager, device, physicalDevice);
		}

		DeviceBuffer<LightSource>* getLightBuffer() {
			return buffer;
		}

		bool lightsChanged() {
			return lightChanged;
		}

	};
}