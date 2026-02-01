#pragma once

namespace vve {
	class MaterialManager : public System {
	private:
		std::vector<MaterialInfo*> materials;
		DeviceBuffer<Material>* buffer;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;

		bool materialCreated = false;

	public:

		bool OnMaterialCreate(Message message);

		void prepareNextFrame();
		bool OnRecordNextFrame(Message message);

		MaterialManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);
		~MaterialManager();

		void freeResources();

		void createMaterialBuffer();

		bool materialChanged();

		DeviceBuffer<Material>* getMaterialBuffer();
	};
}