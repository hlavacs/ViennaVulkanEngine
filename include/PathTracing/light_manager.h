#pragma once

namespace vve {
	class LightManager {
	private:
		std::vector<LightSource*> lights;
		DeviceBuffer<LightSource>* buffer;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		float totalLightWeight = 0.0f;

		LightManager(CommandManager* commandManager, VkDevice device, VkPhysicalDevice physicalDevice) : commandManager(commandManager), device(device), physicalDevice(physicalDevice) {}

		LightSource* addSphereLight(glm::vec4 position, glm::vec4 emission, float radius) {
			LightSource* light = new LightSource();
			float surfaceArea = calculateSphereLightSurfaceArea(radius);
			float sampleWeight = surfaceArea * getLightPower(emission);
			totalLightWeight += sampleWeight;


			light->position = position;
			light->emission = emission;
			light->radius = radius;
			light->lightType = 0;
			light->direction = glm::vec4(0.0, 0.0, 0.0, 0.0);
			light->pdf = 1.0f / surfaceArea;
			light->accumulativeSampleWeight = totalLightWeight;
			lights.push_back(light);
		}

		LightSource* addDiskLight(glm::vec4 position, glm::vec4 emission, glm::vec4 direction, float radius) {
			LightSource* light = new LightSource();
			float surfaceArea = calculateDiskLightSurfaceArea(radius);
			float sampleWeight = surfaceArea * getLightPower(emission);
			totalLightWeight += sampleWeight;

			light->position = position;
			light->emission = emission;
			light->radius = radius;
			light->lightType = 1;
			light->direction = direction;
			light->pdf = 1.0f / surfaceArea;
			light->accumulativeSampleWeight = totalLightWeight;
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
			std::vector<LightSource> lights_data;

			for (LightSource* lightRef : lights) {
				LightSource light = *lightRef;
				light.accumulativeSampleWeight /= totalLightWeight;
				lights_data.push_back(light);
			}

			buffer = new DeviceBuffer<LightSource>(lights_data.data(), lights_data.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, commandManager, device, physicalDevice);
		}

		DeviceBuffer<LightSource>* getLightBuffer() {
			return buffer;
		}

	};
}