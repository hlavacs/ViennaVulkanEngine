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

		LightManager(std::string systemName, Engine& engine, CommandManager* commandManager, VkDevice device, VkPhysicalDevice physicalDevice);

		~LightManager();

		void freeResources();

		bool OnLightCreate(Message message);

		void prepareNextFrame();

		bool OnRecordNextFrame(Message message);
		
		
		void addSphereLight(glm::vec4 position, glm::vec4 emission, float radius);

		void addDiskLight(glm::vec4 position, glm::vec4 emission, glm::vec4 direction, float radius);

		float getLightPower(glm::vec4 emission);

		float calculateSphereLightSurfaceArea(float radius);

		float calculateDiskLightSurfaceArea(float radius);

		void createBuffer();

		DeviceBuffer<LightSource>* getLightBuffer();

		bool lightsChanged();
	};
}