#pragma once

/**
 * @file light_manager.h
 * @brief Light creation and GPU buffer management.
 */

namespace vve {
	/** Manages scene lights and their GPU buffer. */
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

		/**
		 * @param systemName System identifier.
		 * @param engine Engine reference for messaging.
		 * @param commandManager Command manager for staging copies.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 */
		LightManager(std::string systemName, Engine& engine, CommandManager* commandManager, VkDevice device, VkPhysicalDevice physicalDevice);

		/** Release owned Vulkan resources. */
		~LightManager();

		/** Explicitly free Vulkan resources (destructor-safe). */
		void freeResources();

		/**
		 * Handle light creation messages.
		 * @param message Message payload.
		 */
		bool OnLightCreate(Message message);

		/** Prepare light data for the next frame. */
		void prepareNextFrame();

		/**
		 * Record light buffer updates for the next frame.
		 * @param message Message payload.
		 */
		bool OnRecordNextFrame(Message message);
		
		/**
		 * Add a spherical light.
		 * @param position Light position.
		 * @param emission Light emission.
		 * @param radius Sphere radius.
		 */
		void addSphereLight(glm::vec4 position, glm::vec4 emission, float radius);

		/**
		 * Add a disk light.
		 * @param position Light position.
		 * @param emission Light emission.
		 * @param direction Disk normal direction.
		 * @param radius Disk radius.
		 */
		void addDiskLight(glm::vec4 position, glm::vec4 emission, glm::vec4 direction, float radius);

		/**
		 * @param emission Light emission.
		 * @return Light power computed from emission.
		 */
		float getLightPower(glm::vec4 emission);

		/**
		 * @param radius Sphere radius.
		 * @return Surface area of a sphere light.
		 */
		float calculateSphereLightSurfaceArea(float radius);

		/**
		 * @param radius Disk radius.
		 * @return Surface area of a disk light.
		 */
		float calculateDiskLightSurfaceArea(float radius);

		/** Create or rebuild the light buffer. */
		void createBuffer();

		/** @return Pointer to the light buffer. */
		DeviceBuffer<LightSource>* getLightBuffer();

		/** @return True if light data changed since last frame. */
		bool lightsChanged();
	};
}
