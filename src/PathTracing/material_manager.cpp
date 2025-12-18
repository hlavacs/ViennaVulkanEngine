#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	MaterialManager::MaterialManager(VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager) : device(device), physicalDevice(physicalDevice), commandManager(commandManager), buffer(nullptr) {}


	MaterialInfo* MaterialManager::addMaterial(MaterialSlotRGB albedo, MaterialSlotRGB normal, MaterialSlotF roughness, MaterialSlotF metalness, MaterialSlotF ior, MaterialSlotF alpha) {
		MaterialInfo* material = new MaterialInfo();
		material->material.albedoTextureIndex = albedo.textureIndex;
		material->material.albedo = glm::vec4(albedo.rgb, 1.0f);

		material->material.normalTextureIndex = normal.textureIndex;

		material->material.roughnessTextureIndex = roughness.textureIndex;
		material->material.roughness = roughness.f;

		material->material.metalnessTextureIndex = metalness.textureIndex;
		material->material.metalness = metalness.f;

		material->material.iorTextureIndex = ior.textureIndex;
		material->material.ior = ior.f;

		material->material.alphaTextureIndex = alpha.textureIndex;
		material->material.alpha = alpha.f;

		material->materialIndex = materials.size();

		materials.push_back(material);
		return material;
	}

	void MaterialManager::createMaterialBuffer() {
		std::vector<Material> materials_data;



		for (MaterialInfo* material : materials) {
			materials_data.push_back(material->material);
		}

		buffer = new DeviceBuffer<Material>(materials_data.data(), materials_data.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, commandManager, device, physicalDevice);

	}

	DeviceBuffer<Material>* MaterialManager::getMaterialBuffer() {
		return buffer;
	}
}