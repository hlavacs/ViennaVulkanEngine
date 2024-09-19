#ifndef VHVIDEOENCODER_H
#define VHVIDEOENCODER_H

#include "VHHelper.h"
#include "H264ParameterSet.h"

namespace vh {
	class VHVideoEncoder {
	public:
		VkResult init(
			VkPhysicalDevice physicalDevice,
			VkDevice device,
			VmaAllocator allocator,
			uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool,
			uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool,
			const std::vector<VkImageView>& inputImageViews,
			uint32_t width, uint32_t height, uint32_t fps);
		VkResult queueEncode(VkImage currentImage, uint32_t currentImageIx);
		VkResult finishEncode(const char*& data, size_t& size);
		void deinit();

		~VHVideoEncoder() {
			deinit();
		}

	private:
#ifdef VULKAN_VIDEO_ENCODE
        VkResult createVideoSession();
        VkResult allocateVideoSessionMemory();
        VkResult createVideoSessionParameters(uint32_t fps);
		VkResult readBitstreamHeader();
        VkResult allocateOutputBitStream();
        VkResult allocateReferenceImages(uint32_t count);
        VkResult allocateIntermediateImages();
        VkResult createOutputQueryPool();
        VkResult createYUVConversionPipeline(const std::vector<VkImageView>& inputImageViews);
		VkResult initRateControl(VkCommandBuffer cmdBuf, uint32_t fps);
        VkResult transitionImagesInitial(VkCommandBuffer cmdBuf);

        VkResult convertRGBtoYUV(VkImage currentImage, uint32_t currentImageIx);
        VkResult encodeVideoFrame();
        VkResult getOutputVideoPacket(const char*& data, size_t& size);

		bool m_initialized{ false };
		bool m_running{ false };
		VkPhysicalDevice m_physicalDevice;
		VkDevice m_device;
		VmaAllocator m_allocator;
		VkQueue m_computeQueue;
		VkQueue m_encodeQueue;
		uint32_t m_computeQueueFamily;
		uint32_t m_encodeQueueFamily;
		VkCommandPool m_computeCommandPool;
		VkCommandPool m_encodeCommandPool;
		uint32_t m_width;
		uint32_t m_height;

		VkVideoSessionKHR m_videoSession;
		std::vector<VmaAllocation> m_allocations;
		StdVideoH264SequenceParameterSetVui m_vui;
		StdVideoH264SequenceParameterSet m_sps;
		StdVideoH264PictureParameterSet m_pps;
		VkVideoSessionParametersKHR m_videoSessionParameters;
		VkVideoEncodeH264ProfileInfoKHR m_encodeH264ProfileInfoExt;
		VkVideoProfileInfoKHR m_videoProfile;
		VkVideoProfileListInfoKHR m_videoProfileList;
		VkFormat m_chosenDpbImageFormat;
		VkVideoEncodeRateControlModeFlagBitsKHR m_chosenRateControlMode;

		VkVideoEncodeH264RateControlLayerInfoKHR m_encodeH264RateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR };
		VkVideoEncodeRateControlLayerInfoKHR m_encodeRateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR };
		VkVideoEncodeH264RateControlInfoKHR m_encodeH264RateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR };
		VkVideoEncodeRateControlInfoKHR m_encodeRateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR };

		VkDescriptorSetLayout m_computeDescriptorSetLayout;
		VkPipelineLayout m_computePipelineLayout;
		VkPipeline m_computePipeline;
		VkDescriptorPool m_descriptorPool;
		std::vector<VkDescriptorSet> m_computeDescriptorSets;

		VkSemaphore m_interQueueSemaphore;

		VkQueryPool m_queryPool;
		VkBuffer m_bitStreamBuffer;
		VmaAllocation m_bitStreamBufferAllocation;
		std::vector<char> m_bitStreamHeader;
		bool m_bitStreamHeaderPending;
		
		char* m_bitStreamData;

		VkImage m_yuvImage;
		VmaAllocation m_yuvImageAllocation;
		VkImageView m_yuvImageView;
		VkImage m_yuvImageLuma;
		VmaAllocation m_yuvImageLumaAllocation;
		VkImageView m_yuvImageLumaView;
		VkImage m_yuvImageChroma;
		VmaAllocation m_yuvImageChromaAllocation;
		VkImageView m_yuvImageChromaView;

		std::vector<VkImage> m_dpbImages;
		std::vector <VmaAllocation> m_dpbImageAllocations;
		std::vector <VkImageView> m_dpbImageViews;

		uint32_t m_frameCount;

		VkFence m_encodeFinishedFence;
		VkCommandBuffer m_computeCommandBuffer;
		VkCommandBuffer m_encodeCommandBuffer;
#endif
	};
}

#endif
