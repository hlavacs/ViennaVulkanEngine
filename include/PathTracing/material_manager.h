#pragma once

namespace vve {
	class MaterialManager {
	private:
		std::vector<MaterialInfo*> materials;
		DeviceBuffer<Material>* buffer;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;

	public:

		MaterialManager(VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);

		MaterialInfo* addMaterial(MaterialSlotRGB albedo, MaterialSlotRGB normal, MaterialSlotF roughness, MaterialSlotF metalness, MaterialSlotF ior, MaterialSlotF alpha);

		void createMaterialBuffer();

		DeviceBuffer<Material>* getMaterialBuffer();
	};
}