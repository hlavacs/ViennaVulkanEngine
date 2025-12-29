#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	MaterialManager::MaterialManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager):
		System{ systemName, engine }, device(device), physicalDevice(physicalDevice), commandManager(commandManager), buffer(nullptr) 
	{
		createMaterialBuffer();

		engine.RegisterCallbacks({
			{this,  1998, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			{this,  1000, "MATERIAL_CREATE", [this](Message& message) { return OnMaterialCreate(message); } },
			{this,  1998, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } }
			});
	}

	bool MaterialManager::OnMaterialCreate(Message message) {
		materialCreated = true;
		return false;
	}

	bool MaterialManager::OnPrepareNextFrame(Message message) {

		if (materialCreated) {
			std::vector<Material> accumulatedmaterials;
			size_t currentMaterialIndex = 0;

			for (auto [gHandle, mat] : m_registry.GetView<vecs::Handle, vvh::VRTMaterial&>()) {
				Material material;
				material.albedo = glm::vec4(mat().albedo,1.0);
				if (mat().albedoTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().albedoTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.albedoTextureIndex = texture.index;
				}
				else {
					material.albedoTextureIndex = -1;
				}

				if (mat().normalTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().normalTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.normalTextureIndex = texture.index;
				}
				else {
					material.normalTextureIndex = -1;
				}

				material.alpha = mat().alpha;
				if (mat().alphaTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().alphaTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.alphaTextureIndex = texture.index;
				}
				else {
					material.alphaTextureIndex = -1;
				}

				material.roughness = mat().roughness;
				if (mat().roughnessTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().roughnessTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.roughnessTextureIndex = texture.index;
				}
				else {
					material.roughnessTextureIndex = -1;
				}

				material.metalness = mat().metallic;
				if (mat().metallicTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().metallicTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.metalnessTextureIndex = texture.index;
				}
				else {
					material.metalnessTextureIndex = -1;
				}

				material.ior = mat().ior;
				if (mat().iorTextureName != "") {
					TextureHandle tHandle = TextureHandle{ m_engine.GetHandle(mat().iorTextureName) };
					vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
					material.iorTextureIndex = texture.index;
				}
				else {
					material.iorTextureIndex = -1;
				}

				mat().index = accumulatedmaterials.size();
				
				accumulatedmaterials.push_back(material);
			}
			buffer->updateBuffer(accumulatedmaterials.data(), accumulatedmaterials.size());
		}
		return false;
	}

	bool MaterialManager::OnRecordNextFrame(Message message) {
		materialCreated = false;
		return false;
	}

	bool MaterialManager::materialChanged() {
		return materialCreated;
	}

	void MaterialManager::createMaterialBuffer() {
		buffer = new DeviceBuffer<Material>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, commandManager, device, physicalDevice);

	}

	DeviceBuffer<Material>* MaterialManager::getMaterialBuffer() {
		return buffer;
	}
}