
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include "VESystem.h"
#include "VEInclude.h"
#include "VHInclude.h"
#include "VEEngine.h"
#include "VERendererImgui.h"
#include "VEWindowSDL.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui( Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name) 
        : Renderer<ATYPE>(engine, window, name ) {

		engine->RegisterSystem( { 
			  {this, -100, MessageType::INIT, [this](Message message){this->OnInit(message);} }
			, {this, -100, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} }
			, {this, -100, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} }
			, {this, -100, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
			, {this,  100, MessageType::INIT, [this](Message message){this->OnInit2(message);} }
			, {this,  100, MessageType::POLL_EVENTS, [this](Message message){this->OnPollEvents(message);} }
		} );

    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui() {};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit(Message message) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        
        ImGui::StyleColorsDark();  // Setup Dear ImGui style
        //ImGui::StyleColorsLight();
        
        auto window = (WindowSDL<ATYPE>*)m_window;
        ImGui_ImplSDL2_InitForVulkan(window->GetSDLWindow());  // Setup Platform/Renderer backends
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit2(Message message) {

        WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)m_window;
		auto rend = (RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan"));

        //-------------------------------------------------------------------------

        m_mainWindowData.Surface = window->GetSurface();

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(rend->GetPhysicalDevice(), rend->GetQueueFamily(), m_mainWindowData.Surface, &res);
        if (res != VK_TRUE) {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }

        // Select Surface Format
        std::vector<VkFormat> requestSurfaceFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        m_mainWindowData.SurfaceFormat = vh::SelectSurfaceFormat(rend->GetPhysicalDevice(), m_mainWindowData.Surface, requestSurfaceFormats);

        // Select Present Mode
        std::vector<VkPresentModeKHR> requestedPresentModes = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
        m_mainWindowData.PresentMode = vh::SelectPresentMode(rend->GetPhysicalDevice(), m_mainWindowData.Surface, requestedPresentModes);

        auto width = window->GetWidth();
        auto height = window->GetHeight();
        vh::CreateWindowSwapChain(rend->GetPhysicalDevice(), rend->GetDevice(), &m_mainWindowData, rend->GetAllocator(), width, height, window->GetMinImageCount());

        vh::CreateWindowCommandBuffers(rend->GetPhysicalDevice(), rend->GetDevice(), &m_mainWindowData, rend->GetQueueFamily(), rend->GetAllocator());
        vh::CreateDescriptorPool(rend->GetDevice(), &m_descriptorPool);

        //-------------------------------------------------------------------------


        ImGui_ImplVulkan_InitInfo init_info = {};

        init_info.Instance = rend->GetInstance();
        init_info.PhysicalDevice = rend->GetPhysicalDevice();
        init_info.Device = rend->GetDevice();
        init_info.QueueFamily = rend->GetQueueFamily();
        init_info.Queue = rend->GetQueue();
        init_info.PipelineCache = rend->GetPipelineCache();
        init_info.DescriptorPool = m_descriptorPool;
        init_info.RenderPass = m_mainWindowData.RenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = window->GetMinImageCount();
        init_info.ImageCount = m_mainWindowData.ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = rend->GetAllocator();
        init_info.CheckVkResultFn = vh::CheckResult;
        ImGui_ImplVulkan_Init(&init_info);

        // Load Fonts
        // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
        // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
        // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
        // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
        // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
        // - Read 'docs/FONTS.md' for more instructions and details.
        // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
        //io.Fonts->AddFontDefault();
        //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
        //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
        //IM_ASSERT(font != nullptr);

        // Our state
        //bool show_demo_window = true;
        //bool show_another_window = false;
        //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnPrepareNextFrame(Message message) {
        ImGui_ImplVulkan_NewFrame();
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnRenderNextFrame(Message message) {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)m_window;
            m_mainWindowData.ClearValue.color.float32[0] = window->GetClearColor().x * window->GetClearColor().w;
            m_mainWindowData.ClearValue.color.float32[1] = window->GetClearColor().y * window->GetClearColor().w;
            m_mainWindowData.ClearValue.color.float32[2] = window->GetClearColor().z * window->GetClearColor().w;
            m_mainWindowData.ClearValue.color.float32[3] = window->GetClearColor().w;
            
            auto wd = &m_mainWindowData;

            VkResult err;

		    auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));

            VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
            VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
            err = vkAcquireNextImageKHR(rend->GetDevice(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            {
                window->SetSwapChainRebuild(true);
                return;
            }
            vh::CheckResult(err);

            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
            {
                vh::CheckResult(vkWaitForFences(rend->GetDevice(), 1, &fd->Fence, VK_TRUE, UINT64_MAX));
                vh::CheckResult(vkResetFences(rend->GetDevice(), 1, &fd->Fence));
            }
            {
                vh::CheckResult(vkResetCommandPool(rend->GetDevice(), fd->CommandPool, 0));
                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vh::CheckResult(vkBeginCommandBuffer(fd->CommandBuffer, &info));
            }
            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = wd->RenderPass;
                info.framebuffer = fd->Framebuffer;
                info.renderArea.extent.width = wd->Width;
                info.renderArea.extent.height = wd->Height;
                info.clearValueCount = 1;
                info.pClearValues = &wd->ClearValue;
                vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            // Record dear imgui primitives into command buffer
            ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer); ////!!!!!!!!!!!!!!

            // Submit command buffer
            vkCmdEndRenderPass(fd->CommandBuffer);
            {
                VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &image_acquired_semaphore;
                info.pWaitDstStageMask = &wait_stage;
                info.commandBufferCount = 1;
                info.pCommandBuffers = &fd->CommandBuffer;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores = &render_complete_semaphore;

                vh::CheckResult(vkEndCommandBuffer(fd->CommandBuffer));
                vh::CheckResult(vkQueueSubmit(rend->GetQueue(), 1, &info, fd->Fence));
            }
        }

        if (!is_minimized) {
            WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)m_window;
		    auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));

            if (window->GetSwapChainRebuild())
                return;
            VkSemaphore render_complete_semaphore = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].RenderCompleteSemaphore;
            VkPresentInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &render_complete_semaphore;
            info.swapchainCount = 1;
            info.pSwapchains = &m_mainWindowData.Swapchain;
            info.pImageIndices = &m_mainWindowData.FrameIndex;
            VkResult err = vkQueuePresentKHR(rend->GetQueue(), &info);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            {
                window->SetSwapChainRebuild(true);
                return;
            }
            vh::CheckResult(err);
            m_mainWindowData.SemaphoreIndex = (m_mainWindowData.SemaphoreIndex + 1) % m_mainWindowData.SemaphoreCount; // Now we can use the next set of semaphores        }
        }
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnPollEvents(Message message) {
        WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)m_window;
        if (window->GetWidth() > 0 && window->GetHeight() > 0 && (window->GetSwapChainRebuild() || m_mainWindowData.Width != window->GetWidth() || m_mainWindowData.Height != window->GetHeight()))
        {
		    auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));

            ImGui_ImplVulkan_SetMinImageCount(window->GetMinImageCount());
            ImGui_ImplVulkanH_CreateOrResizeWindow(rend->GetInstance(), rend->GetPhysicalDevice()
                , rend->GetDevice(), &m_mainWindowData, rend->GetQueueFamily()
                , rend->GetAllocator(), window->GetWidth(), window->GetHeight(), window->GetMinImageCount());

            m_mainWindowData.FrameIndex = 0;
            window->SetSwapChainRebuild(false);
        }
    }
    
   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnQuit(Message message) {
		auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));

        vh::CheckResult(vkDeviceWaitIdle(rend->GetDevice()));
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(rend->GetDevice(), m_descriptorPool, rend->GetAllocator());
        ImGui_ImplVulkanH_DestroyWindow(rend->GetInstance(), rend->GetDevice(), &m_mainWindowData, rend->GetAllocator());
    }

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

