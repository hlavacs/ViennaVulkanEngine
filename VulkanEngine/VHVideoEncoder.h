#ifndef VHVIDEOENCODER_H
#define VHVIDEOENCODER_H

#include "VHHelper.h"

namespace vh {
	class VHVideoEncoder {
	public:
		VkResult init(VkDevice device, VmaAllocator allocator, uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool, uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool, uint32_t width, uint32_t height);
		VkResult queueEncode(VkImageView imageView);

		void deinit();

		VkResult queueCopy(VkDevice _device, VmaAllocator _allocator, VkQueue graphicsQueue, VkCommandPool _commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
			uint32_t _width, uint32_t _height)
		{
			assert(!running);
			allocator = _allocator;
			device = _device;
			commandPool = _commandPool;

			VHCHECKRESULT(allocateCommandBuffer());

			uint32_t imageSize = _width * _height * 4;
			if (imageSize != width * height * 4) {
				freeBuffer();
				VHCHECKRESULT(vhBufCreateBuffer(allocator, imageSize,
					VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU,
					&stagingBuffer, &stagingBufferAllocation));
			}
			width = _width;
			height = _height;

			VHCHECKRESULT(vhCmdBeginCommandBuffer(device, cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT));

			VHCHECKRESULT(vhBufTransitionImageLayout(device, graphicsQueue, cmdBuffer,
				image, format, aspect, 1, 1,
				layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));

			std::vector<VkBufferImageCopy> regions;
			VkBufferImageCopy region = {};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = aspect;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { width, height, 1 };
			regions.push_back(region);
			vkCmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, (uint32_t)regions.size(), regions.data());

			VHCHECKRESULT(vhBufTransitionImageLayout(device, graphicsQueue, cmdBuffer,
				image, format, aspect, 1, 1,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout));

			VHCHECKRESULT(vkEndCommandBuffer(cmdBuffer));
			VHCHECKRESULT(vhCmdSubmitCommandBuffer(device, graphicsQueue, cmdBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, waitFence));

			bufferData = new uint8_t[imageSize];

			backgroundCopy = std::thread([this] {
				vkWaitForFences(device, 1, &waitFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
				vmaInvalidateAllocation(allocator, stagingBufferAllocation, 0, VK_WHOLE_SIZE);
				void* data;
				vmaMapMemory(allocator, stagingBufferAllocation, &data);
				memcpy(bufferData, data, (size_t)(width * height * 4));
				vmaUnmapMemory(allocator, stagingBufferAllocation);
				});

			running = true;
			return VK_SUCCESS;
		}

		VkResult finishCopy(unsigned char*& _bufferData,
			uint32_t& _width,
			uint32_t& _height) {
			if (!running)
				return VK_NOT_READY;

			backgroundCopy.join();

			freeCommandBuffer();

			_bufferData = bufferData;
			_width = width;
			_height = height;
			running = false;
			return VK_SUCCESS;
		}

		~VHVideoEncoder() {
			if (running) {
				backgroundCopy.join();
				delete[] bufferData;
			}
			freeBuffer();
			freeCommandBuffer();

			deinit();
		}

	private:
		void initRateControl(VkCommandBuffer cmdBuf, uint32_t qp);

		VkResult allocateCommandBuffer() {
			if (cmdbuf_allocated)
				return VK_SUCCESS;

			VHCHECKRESULT(vhCmdCreateCommandBuffers(device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &cmdBuffer));

			VkFenceCreateInfo fenceInfo = {};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			vkCreateFence(device, &fenceInfo, nullptr, &waitFence);

			cmdbuf_allocated = true;
			return VK_SUCCESS;
		}

		void freeBuffer() {
			if (width != 0 && height != 0) {
				vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
			}
		}

		void freeCommandBuffer() {
			if (cmdbuf_allocated) {
				vkDestroyFence(device, waitFence, nullptr);
				vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
				cmdbuf_allocated = false;
			}
		}

		bool m_initialized{ false };
		VkDevice m_device;
		VmaAllocator m_allocator;
		VkQueue m_computeQueue;
		VkQueue m_encodeQueue;
		VkCommandPool m_computeCommandPool;
		VkCommandPool m_encodeCommandPool;
		uint32_t m_width;
		uint32_t m_height;

		VkVideoSessionKHR m_videoSession;
		std::vector<VmaAllocation> m_allocations;
		StdVideoH264SequenceParameterSet m_sps;
		StdVideoH264PictureParameterSet m_pps;
		VkVideoSessionParametersKHR m_videoSessionParameters;

		VkDescriptorSetLayout m_computeDescriptorSetLayout;
		VkPipelineLayout m_computePipelineLayout;
		VkPipeline m_computePipeline;
		VkDescriptorPool m_descriptorPool;
		std::vector<VkDescriptorSet> m_computeDescriptorSets;

		VkQueryPool m_queryPool;
		VkBuffer m_bitStreamBuffer;
		VmaAllocation m_bitStreamBufferAllocation;
		std::ofstream m_outfile;

		VkImage m_srcImage;
		VmaAllocation m_srcImageAllocation;
		VkImageView m_srcImageView;
		VkImageView m_srcImageView0;
		VkImageView m_srcImageView1;

		VkImage m_dpbImage;
		VmaAllocation m_dpbImageAllocation;
		VkImageView m_dpbImageView;

		uint32_t m_frameCount;


		bool cmdbuf_allocated{ false };
		bool running{ false };
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		unsigned char* bufferData;

		VkDevice device;
		VkCommandPool commandPool;
		VkCommandBuffer cmdBuffer;
		VkFence waitFence;

		VmaAllocator allocator;
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;

		std::thread backgroundCopy;
	};
}

#endif
