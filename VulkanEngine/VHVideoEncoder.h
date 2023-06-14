#ifndef VHVIDEOENCODER_H
#define VHVIDEOENCODER_H

#include "VHHelper.h"

namespace vh {
	class VHVideoEncoder {
	public:
		VkResult init(VkDevice device, VmaAllocator allocator, uint32_t computeQueueFamily, VkQueue computeQueue, VkCommandPool computeCommandPool, uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool, uint32_t width, uint32_t height);
		VkResult queueEncode(VkImageView inputImageView);
		void deinit();

		~VHVideoEncoder() {
			deinit();
		}

	private:
        VkResult createVideoSession();
        VkResult allocateVideoSessionMemory();
        VkResult createVideoSessionParameters();
        VkResult allocateOutputBitStream();
        VkResult allocateReferenceImages();
        VkResult allocateIntermediateImages();
        VkResult createOutputQueryPool();
        VkResult createYUVConversionPipeline();
		VkResult initRateControl(VkCommandBuffer cmdBuf, uint32_t qp);
        VkResult transitionImagesInitial(VkCommandBuffer cmdBuf);

        VkResult convertRGBtoYUV(VkImageView inputImageView);
        VkResult encodeVideoFrame();
        VkResult readOutputVideoPacket();

		bool m_initialized{ false };
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
		StdVideoH264SequenceParameterSet m_sps;
		StdVideoH264PictureParameterSet m_pps;
		VkVideoSessionParametersKHR m_videoSessionParameters;
		VkVideoEncodeH264ProfileInfoEXT m_encodeH264ProfileInfoExt;
		VkVideoProfileInfoKHR m_videoProfile;

		VkDescriptorSetLayout m_computeDescriptorSetLayout;
		VkPipelineLayout m_computePipelineLayout;
		VkPipeline m_computePipeline;
		VkDescriptorPool m_descriptorPool;
		std::vector<VkDescriptorSet> m_computeDescriptorSets;

		VkQueryPool m_queryPool;
		VkBuffer m_bitStreamBuffer;
		VmaAllocation m_bitStreamBufferAllocation;
		std::ofstream m_outfile;

		VkImage m_yuvImage;
		VmaAllocation m_yuvImageAllocation;
		VkImageView m_yuvImageView;
		VkImageView m_yuvImagePlane0View;
		VkImage m_yuvImageChroma;
		VmaAllocation m_yuvImageChromaAllocation;
		VkImageView m_yuvImageChromaView;


		VkImage m_dpbImage;
		VmaAllocation m_dpbImageAllocation;
		VkImageView m_dpbImageView;

		uint32_t m_frameCount;
	};
}

#endif
