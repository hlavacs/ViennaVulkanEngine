#pragma once

/**
 * @file material_manager.h
 * @brief Material creation and GPU buffer management.
 */

namespace vve {
	/** Manages materials and their GPU buffer. */
	class MaterialManager : public System {
	private:
		std::vector<MaterialInfo*> materials;
		DeviceBuffer<Material>* buffer;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;

		bool materialCreated = false;

	public:

		/** Handle material creation messages. */
		bool OnMaterialCreate(Message message);

		/** Prepare material data for the next frame. */
		void prepareNextFrame();
		/** Record material buffer updates for the next frame. */
		bool OnRecordNextFrame(Message message);

		/**
		 * @param systemName System identifier.
		 * @param engine Engine reference for messaging.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 * @param commandManager Command manager for staging copies.
		 */
		MaterialManager(std::string systemName, Engine& engine, VkDevice& device, VkPhysicalDevice& physicalDevice, CommandManager* commandManager);
		/** Release owned Vulkan resources. */
		~MaterialManager();

		/** Explicitly free Vulkan resources (destructor-safe). */
		void freeResources();

		/** Create or rebuild the material buffer. */
		void createMaterialBuffer();

		/** @return True if material data changed since last frame. */
		bool materialChanged();

		/** @return Pointer to the material buffer. */
		DeviceBuffer<Material>* getMaterialBuffer();
	};
}
