#pragma once

/**
 * @file render_target.h
 * @brief Render target images and clear state.
 */

namespace vve {
	/** Wrapper for per-frame render target images. */
	template <typename T>
	class RenderTargetBuffer {
	private:
		std::vector<DeviceBuffer<T>*> buffers;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		T initValue;
	public:

		/**
		 * Create a render target with a default clear color.
		 * @param width Target width in pixels.
		 * @param height Target height in pixels.
		 * @param format Image format.
		 * @param usage Image usage flags.
		 * @param aspectFlags Image aspect flags.
		 * @param commandManager Command manager for image transitions.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 */
		RenderTargetBuffer(uint32_t width, uint32_t height, T initValue, VkBufferUsageFlags usage, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
			commandManager(commandManager), device(device), physicalDevice(physicalDevice), initValue(initValue) {

			std::vector<T> initVector(width * height, initValue);

			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				DeviceBuffer<T>* buffer = new DeviceBuffer<T>(initVector.data(), width * height, usage, 0, commandManager, device, physicalDevice);
				buffers.push_back(buffer);
			}

		}


		/** Release owned image resources. */
		~RenderTargetBuffer() {
			for (DeviceBuffer<T>* buffer : buffers) {
				delete buffer;
			}
		}

		/**
		 * @param currentFrame Frame index.
		 * @return Image for the given frame index.
		 */
		DeviceBuffer<T>* getBuffer(int currentFrame) {
			return buffers[currentFrame];
		}

		/**
		 * Recreate all images with a new size.
		 * @param width New width in pixels.
		 * @param height New height in pixels.
		 */
		void recreateRenderTarget(uint32_t width, uint32_t height) {
			for (DeviceBuffer<T>* buffer : buffers) {
				std::vector<T> initVector(width * height, initValue);
				buffer->updateBuffer(initVector.data(), width * height);
			}
		}

		PerFrameDescriptorPlacment* getDescriptorInput(size_t binding, VkShaderStageFlags stageFlags) {

			std::vector<DescriptorInput*> descriptors;

			for (DeviceBuffer<T>* buffer : buffers) {
				DescriptorBufferInput* descriptorInput = new DescriptorBufferInput(buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageFlags);
				descriptors.push_back(descriptorInput);
			}

			return new PerFrameDescriptorPlacment(descriptors, binding);
		}
	};


	//uses host buffer for debugging!
	template <typename T>
	class RenderTargetBufferDebug {
	private:
		std::vector<HostBuffer<T>*> buffers;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		T initValue;
	public:

		/**
		 * Create a render target with a default clear color.
		 * @param width Target width in pixels.
		 * @param height Target height in pixels.
		 * @param format Image format.
		 * @param usage Image usage flags.
		 * @param aspectFlags Image aspect flags.
		 * @param commandManager Command manager for image transitions.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 */
		RenderTargetBufferDebug(uint32_t width, uint32_t height, T initValue, VkBufferUsageFlags usage, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
			commandManager(commandManager), device(device), physicalDevice(physicalDevice), initValue(initValue) {

			std::vector<T> initVector(width * height, initValue);

			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				HostBuffer<T>* buffer = new HostBuffer<T>(initVector.data(), width * height, usage, 0, device, physicalDevice);
				buffers.push_back(buffer);
			}

		}


		/** Release owned image resources. */
		~RenderTargetBufferDebug() {
			for (HostBuffer<T>* buffer : buffers) {
				delete buffer;
			}
		}

		/**
		 * @param currentFrame Frame index.
		 * @return Image for the given frame index.
		 */
		DeviceBuffer<T>* getBuffer(int currentFrame) {
			return buffers[currentFrame];
		}

		std::vector<T> getData(int currentFrame) {
			return buffers[currentFrame]->getData();
		}

		/**
		 * Recreate all images with a new size.
		 * @param width New width in pixels.
		 * @param height New height in pixels.
		 */
		void recreateRenderTarget(uint32_t width, uint32_t height) {
			for (HostBuffer<T>* buffer : buffers) {
				std::vector<T> initVector(width * height, initValue);
				buffer->updateBuffer(initVector.data(), width * height);
			}
		}

		PerFrameDescriptorPlacment* getDescriptorInput(size_t binding, VkShaderStageFlags stageFlags) {

			std::vector<DescriptorInput*> descriptors;

			for (HostBuffer<T>* buffer : buffers) {
				DescriptorBufferInput* descriptorInput = new DescriptorBufferInput(buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageFlags);
				descriptors.push_back(descriptorInput);
			}

			return new PerFrameDescriptorPlacment(descriptors, binding);
		}
	};



}
