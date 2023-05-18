#include "H264ParameterSet.h"
#include "VHVideoEncoder.h"

namespace vh {

    VkResult VHVideoEncoder::init(VkDevice device, VmaAllocator allocator, uint32_t encodeQueueFamily, VkQueue encodeQueue, VkCommandPool encodeCommandPool, uint32_t width, uint32_t height)
    {
        if (m_initialized) {
            return VK_SUCCESS;
        }
        m_device = device;
        m_allocator = allocator;
        m_encodeQueue = encodeQueue;
        m_encodeCommandPool = encodeCommandPool;
        m_width = width;
        m_height = height;

        VkVideoEncodeH264ProfileInfoEXT encodeH264ProfileInfoExt = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_EXT };
        encodeH264ProfileInfoExt.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_BASELINE;

        VkVideoProfileInfoKHR videoProfile = { VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR };
        videoProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_EXT;
        videoProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
        videoProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        videoProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        videoProfile.pNext = &encodeH264ProfileInfoExt;

        static const VkExtensionProperties h264StdExtensionVersion = { VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION };
        VkVideoSessionCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR };
        createInfo.pVideoProfile = &videoProfile;
        createInfo.queueFamilyIndex = encodeQueueFamily;
        createInfo.pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        createInfo.maxCodedExtent = { m_width, m_height };
        createInfo.maxDpbSlots = 16;
        createInfo.maxActiveReferencePictures = 16;
        createInfo.referencePictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        createInfo.pStdHeaderVersion = &h264StdExtensionVersion;

        VHCHECKRESULT(vkCreateVideoSessionKHR(m_device, &createInfo, nullptr, &m_videoSession));


        uint32_t videoSessionMemoryRequirementsCount = 0;
        VHCHECKRESULT(vkGetVideoSessionMemoryRequirementsKHR(m_device, m_videoSession,
            &videoSessionMemoryRequirementsCount, nullptr));
        std::vector<VkVideoSessionMemoryRequirementsKHR> encodeSessionMemoryRequirements(videoSessionMemoryRequirementsCount);
        for (uint32_t i = 0; i < videoSessionMemoryRequirementsCount; i++) {
            memset(&encodeSessionMemoryRequirements[i], 0, sizeof(VkVideoSessionMemoryRequirementsKHR));
            encodeSessionMemoryRequirements[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
        }
        VHCHECKRESULT(vkGetVideoSessionMemoryRequirementsKHR(m_device, m_videoSession,
            &videoSessionMemoryRequirementsCount,
            encodeSessionMemoryRequirements.data()));

        std::vector<VkBindVideoSessionMemoryInfoKHR> encodeSessionBindMemory(videoSessionMemoryRequirementsCount);
        m_allocations.resize(videoSessionMemoryRequirementsCount);
        for (uint32_t memIdx = 0; memIdx < videoSessionMemoryRequirementsCount; memIdx++) {
            VmaAllocationCreateInfo allocCreateInfo = {};
            allocCreateInfo.memoryTypeBits = encodeSessionMemoryRequirements[memIdx].memoryRequirements.memoryTypeBits;

            VmaAllocationInfo allocInfo;
            VHCHECKRESULT(vmaAllocateMemory(m_allocator, &encodeSessionMemoryRequirements[memIdx].memoryRequirements, &allocCreateInfo, &m_allocations[memIdx], &allocInfo));

            encodeSessionBindMemory[memIdx].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
            encodeSessionBindMemory[memIdx].pNext = nullptr;
            encodeSessionBindMemory[memIdx].memory = allocInfo.deviceMemory;

            encodeSessionBindMemory[memIdx].memoryBindIndex = encodeSessionMemoryRequirements[memIdx].memoryBindIndex;
            encodeSessionBindMemory[memIdx].memoryOffset = allocInfo.offset;
            encodeSessionBindMemory[memIdx].memorySize = allocInfo.size;
        }
        VHCHECKRESULT(vkBindVideoSessionMemoryKHR(m_device, m_videoSession, videoSessionMemoryRequirementsCount,
            encodeSessionBindMemory.data()));

        m_sps = h264::getStdVideoH264SequenceParameterSet(m_width, m_height, nullptr);
        m_pps = h264::getStdVideoH264PictureParameterSet();

        VkVideoEncodeH264SessionParametersAddInfoEXT encodeH264SessionParametersAddInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_EXT };
        encodeH264SessionParametersAddInfo.pNext = nullptr;
        encodeH264SessionParametersAddInfo.stdSPSCount = 1;
        encodeH264SessionParametersAddInfo.pStdSPSs = &m_sps;
        encodeH264SessionParametersAddInfo.stdPPSCount = 1;
        encodeH264SessionParametersAddInfo.pStdPPSs = &m_pps;

        VkVideoEncodeH264SessionParametersCreateInfoEXT encodeH264SessionParametersCreateInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_EXT };
        encodeH264SessionParametersCreateInfo.pNext = nullptr;
        encodeH264SessionParametersCreateInfo.maxStdSPSCount = 1;
        encodeH264SessionParametersCreateInfo.maxStdPPSCount = 1;
        encodeH264SessionParametersCreateInfo.pParametersAddInfo = &encodeH264SessionParametersAddInfo;

        VkVideoSessionParametersCreateInfoKHR sessionParametersCreateInfo = { VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR };
        sessionParametersCreateInfo.pNext = &encodeH264SessionParametersCreateInfo;
        sessionParametersCreateInfo.videoSessionParametersTemplate = nullptr;
        sessionParametersCreateInfo.videoSession = m_videoSession;

        VHCHECKRESULT(vkCreateVideoSessionParametersKHR(m_device, &sessionParametersCreateInfo, nullptr, &m_videoSessionParameters));

        VHCHECKRESULT(vhBufCreateBuffer(m_allocator, 4 * 1024 * 1024,
            VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR, VMA_MEMORY_USAGE_GPU_TO_CPU,
            &m_bitStreamBuffer, &m_bitStreamBufferAllocation));
        
        VkImageCreateInfo tmpImgCreateInfo;
        tmpImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        tmpImgCreateInfo.pNext = &videoProfile;
        tmpImgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        tmpImgCreateInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        tmpImgCreateInfo.extent = { m_width, m_height, 1 };
        tmpImgCreateInfo.mipLevels = 1;
        tmpImgCreateInfo.arrayLayers = 1;
        tmpImgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        tmpImgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        tmpImgCreateInfo.usage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR; // DPB ONLY
        tmpImgCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT; // VK_SHARING_MODE_EXCLUSIVE here makes it not check for queueFamily
        tmpImgCreateInfo.queueFamilyIndexCount = 1;
        tmpImgCreateInfo.pQueueFamilyIndices = &encodeQueueFamily;
        tmpImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        tmpImgCreateInfo.flags = 0;
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_dpbImage, &m_dpbImageAllocation, nullptr));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_dpbImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_dpbImageView));

        tmpImgCreateInfo.usage = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
        tmpImgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VHCHECKRESULT(vmaCreateImage(m_allocator, &tmpImgCreateInfo, &allocInfo, &m_srcImage, &m_srcImageAllocation, nullptr));
        VHCHECKRESULT(vhBufCreateImageView(m_device, m_srcImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT, &m_srcImageView));


        VkQueryPoolVideoEncodeFeedbackCreateInfoKHR queryPoolVideoEncodeFeedbackCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR };
        queryPoolVideoEncodeFeedbackCreateInfo.encodeFeedbackFlags = VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR | VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;
        queryPoolVideoEncodeFeedbackCreateInfo.pNext = &videoProfile;
        VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        queryPoolCreateInfo.queryType = VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR;
        queryPoolCreateInfo.queryCount = 1;
        queryPoolCreateInfo.pNext = &queryPoolVideoEncodeFeedbackCreateInfo;
        VHCHECKRESULT(vkCreateQueryPool(m_device, &queryPoolCreateInfo, NULL, &m_queryPool));


        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_encodeCommandPool);
        initRateControl(cmdBuffer, 20);
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuffer,
            m_dpbImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR));
        VHCHECKRESULT(vhBufTransitionImageLayout(m_device, m_encodeQueue, cmdBuffer,
                m_srcImage, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR));
        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_encodeQueue, m_encodeCommandPool, cmdBuffer));

        m_outfile.open("hwenc.264", std::ios::binary);
        // Hardcoded for 352x288, 25 fps
        // need to implement an encoder for SPS and PPS or use an external one
        const uint8_t sps[] =
        { 0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x29, 0xac, 0xca, 0x81, 0x60, 0x96, 0x40 };
        h264::encodeSps(m_sps).writeTo(m_outfile);
        //m_outfile.write(reinterpret_cast<const char*>(sps), sizeof(sps));

        const uint8_t pps[] =
        { 0x00, 0x00, 0x00, 0x01, 0x68, 0xee, 0x3c, 0xb0 };
        h264::encodePps(m_pps).writeTo(m_outfile);
        //m_outfile.write(reinterpret_cast<const char*>(pps), sizeof(pps));

        m_frameCount = 0;
        m_initialized = true;
        return VK_SUCCESS;
    }

    VkResult VHVideoEncoder::queueEncode(VkImage image)
    {
        VkCommandBuffer cmdBuffer = vhCmdBeginSingleTimeCommands(m_device, m_encodeCommandPool);

        VkVideoBeginCodingInfoKHR encodeBeginInfo = { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        encodeBeginInfo.videoSession = m_videoSession;
        encodeBeginInfo.videoSessionParameters = m_videoSessionParameters;
        encodeBeginInfo.referenceSlotCount = 0;
        encodeBeginInfo.pReferenceSlots = nullptr;
        vkCmdBeginVideoCodingKHR(cmdBuffer, &encodeBeginInfo);

        VkVideoPictureResourceInfoKHR inputPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        inputPicResource.imageViewBinding = m_srcImageView;
        inputPicResource.codedOffset = { 0, 0 };
        inputPicResource.codedExtent = { m_width, m_height };
        inputPicResource.baseArrayLayer = 0;

        VkVideoPictureResourceInfoKHR dpbPicResource = { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
        dpbPicResource.imageViewBinding = m_dpbImageView;
        dpbPicResource.codedOffset = { 0, 0 };
        dpbPicResource.codedExtent = { m_width, m_height };
        dpbPicResource.baseArrayLayer = 0;

        VkVideoReferenceSlotInfoKHR referenceSlot{ VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR };
        referenceSlot.pNext = nullptr;
        referenceSlot.slotIndex = 0;
        referenceSlot.pPictureResource = &dpbPicResource;

        h264::IntraFrameInfo intraFrameInfo(m_frameCount, m_width, m_height,
            m_sps,
            m_pps,
            m_frameCount == 0);
        VkVideoEncodeH264VclFrameInfoEXT* encodeH264FrameInfo = intraFrameInfo.getEncodeH264FrameInfo();

        VkVideoEncodeInfoKHR videoEncodeInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR };
        videoEncodeInfo.pNext = encodeH264FrameInfo;
        videoEncodeInfo.qualityLevel = 0;
        videoEncodeInfo.dstBuffer = m_bitStreamBuffer;
        videoEncodeInfo.dstBufferOffset = 0;
        videoEncodeInfo.srcPictureResource = inputPicResource;
        videoEncodeInfo.pSetupReferenceSlot = &referenceSlot;

        const uint32_t querySlotId = 0;
        vkCmdResetQueryPool(cmdBuffer, m_queryPool, querySlotId, 1);
        vkCmdBeginQuery(cmdBuffer, m_queryPool, querySlotId, VkQueryControlFlags());
        vkCmdEncodeVideoKHR(cmdBuffer, &videoEncodeInfo);
        vkCmdEndQuery(cmdBuffer, m_queryPool, querySlotId);

        VkVideoEndCodingInfoKHR encodeEndInfo = { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };
        vkCmdEndVideoCodingKHR(cmdBuffer, &encodeEndInfo);

        VHCHECKRESULT(vhCmdEndSingleTimeCommands(m_device, m_encodeQueue, m_encodeCommandPool, cmdBuffer));

        struct nvVideoEncodeStatus {
            uint32_t bitstreamStartOffset;
            uint32_t bitstreamSize;
            VkQueryResultStatusKHR status;
        };
        nvVideoEncodeStatus encodeResult[2]; // 2nd slot is non vcl data
        memset(&encodeResult, 0, sizeof(encodeResult));
        VHCHECKRESULT(vkGetQueryPoolResults(m_device, m_queryPool, querySlotId, 1, sizeof(nvVideoEncodeStatus),
            &encodeResult[0], sizeof(nvVideoEncodeStatus), VK_QUERY_RESULT_WITH_STATUS_BIT_KHR | VK_QUERY_RESULT_WAIT_BIT));

        std::cout << "status: " << encodeResult[0].status << " offset: " << encodeResult[0].bitstreamStartOffset << " size: " << encodeResult[0].bitstreamSize << std::endl;
        vmaInvalidateAllocation(m_allocator, m_bitStreamBufferAllocation, encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        void* data;
        vmaMapMemory(m_allocator, m_bitStreamBufferAllocation, &data);
        m_outfile.write(reinterpret_cast<const char*>(data) + encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        //memcpy(bufferData, reinterpret_cast<uint8_t*>(data) + encodeResult[0].bitstreamStartOffset, encodeResult[0].bitstreamSize);
        vmaUnmapMemory(m_allocator, m_bitStreamBufferAllocation);


        //fwrite(data + bitstreamOffset + encodeResult[0].bitstreamStartOffset, 1, encodeResult[0].bitstreamSize, encodeConfig->outputVid);


        m_frameCount++;

        return VK_SUCCESS;
    }


    void VHVideoEncoder::deinit()
    {
        if (!m_initialized) {
            return;
        }

        vkDestroyVideoSessionParametersKHR(m_device, m_videoSessionParameters, nullptr);
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
        vmaDestroyBuffer(m_allocator, m_bitStreamBuffer, m_bitStreamBufferAllocation);
        vkDestroyImageView(m_device, m_srcImageView, nullptr);
        vmaDestroyImage(m_allocator, m_srcImage, m_srcImageAllocation);
        vkDestroyImageView(m_device, m_dpbImageView, nullptr);
        vmaDestroyImage(m_allocator, m_dpbImage, m_dpbImageAllocation);
        vkDestroyVideoSessionKHR(m_device, m_videoSession, nullptr);
        for (VmaAllocation& allocation : m_allocations) {
            vmaFreeMemory(m_allocator, allocation);
        }
        m_allocations.clear();
        m_outfile.close();

        m_initialized = false;
    }


    void VHVideoEncoder::initRateControl(VkCommandBuffer cmdBuf, uint32_t qp)
    {
        VkVideoBeginCodingInfoKHR encodeBeginInfo = { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
        encodeBeginInfo.videoSession = m_videoSession;
        encodeBeginInfo.videoSessionParameters = m_videoSessionParameters;

        VkVideoEncodeH264FrameSizeEXT encodeH264FrameSize;
        encodeH264FrameSize.frameISize = 0;

        VkVideoEncodeH264QpEXT encodeH264Qp;
        encodeH264Qp.qpI = qp;

        VkVideoEncodeH264RateControlLayerInfoEXT encodeH264RateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_EXT };
        encodeH264RateControlLayerInfo.useInitialRcQp = VK_TRUE;
        encodeH264RateControlLayerInfo.initialRcQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMinQp = VK_TRUE;
        encodeH264RateControlLayerInfo.minQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxQp = VK_TRUE;
        encodeH264RateControlLayerInfo.maxQp = encodeH264Qp;
        encodeH264RateControlLayerInfo.useMaxFrameSize = VK_TRUE;
        encodeH264RateControlLayerInfo.maxFrameSize = encodeH264FrameSize;
        encodeH264RateControlLayerInfo.temporalLayerId = 0;

        VkVideoEncodeRateControlLayerInfoKHR encodeRateControlLayerInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR };
        encodeRateControlLayerInfo.pNext = &encodeH264RateControlLayerInfo;

        VkVideoEncodeH264RateControlInfoEXT encodeH264RateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_EXT };
        encodeH264RateControlInfo.gopFrameCount = UINT32_MAX;
        encodeH264RateControlInfo.idrPeriod = UINT32_MAX;
        encodeH264RateControlInfo.consecutiveBFrameCount = 0;
        encodeH264RateControlInfo.temporalLayerCount = 1;
        encodeH264RateControlInfo.rateControlStructure = VK_VIDEO_ENCODE_H264_RATE_CONTROL_STRUCTURE_UNKNOWN_EXT;

        VkVideoEncodeRateControlInfoKHR encodeRateControlInfo = { VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR };
        encodeRateControlInfo.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
        encodeRateControlInfo.pNext = &encodeH264RateControlInfo;
        encodeRateControlInfo.layerCount = 1;
        encodeRateControlInfo.pLayers = &encodeRateControlLayerInfo;

        VkVideoCodingControlInfoKHR codingControlInfo = { VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR };
        codingControlInfo.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR | VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR;
        codingControlInfo.pNext = &encodeRateControlInfo;

        VkVideoEndCodingInfoKHR encodeEndInfo = { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR };

        // Reset the video session before first use and apply QP values.
        vkCmdBeginVideoCodingKHR(cmdBuf, &encodeBeginInfo);
        vkCmdControlVideoCodingKHR(cmdBuf, &codingControlInfo);
        vkCmdEndVideoCodingKHR(cmdBuf, &encodeEndInfo);
    }
};
