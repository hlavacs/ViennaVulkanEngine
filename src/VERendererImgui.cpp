
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include "VESystem.h"
#include "VEInclude.h"
#include "VHDevice.h"
#include "VEEngine.h"
#include "VERendererImgui.h"
#include "VEWindowSDL.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui( Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name) 
        : Renderer<ATYPE>(engine, window, name ) {

        engine->RegisterSystem( this, 100, {MessageType::INIT} ); //init after window

        engine->RegisterSystem( this, -100 //prepare before window
            , {MessageType::PREPARE_NEXT_FRAME, MessageType::RENDER_NEXT_FRAME, MessageType::QUIT} );

    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui(){
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_engine->GetState().m_device, m_descriptorPool, m_engine->GetState().m_allocator);
    };

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit(Message message) {
        WindowSDL<ATYPE>* sdlwindow = (WindowSDL<ATYPE>*)m_window;
        auto state = m_engine->GetState();
        vh::CreateWindowCommandBuffers(state.m_physicalDevice, state.m_device, &sdlwindow->m_mainWindowData, state.m_queueFamily, state.m_allocator);
        vh::CreateDescriptorPool(m_engine->GetState().m_device, &m_descriptorPool);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        //ImGuiIO& io = ImGui::GetIO(); (void)io;
        m_io = &ImGui::GetIO();
        m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForVulkan(sdlwindow->m_window);
        ImGui_ImplVulkan_InitInfo init_info = {};

        init_info.Instance = state.m_instance;
        init_info.PhysicalDevice = state.m_physicalDevice;
        init_info.Device = state.m_device;
        init_info.QueueFamily = state.m_queueFamily;
        init_info.Queue = state.m_queue;
        init_info.PipelineCache = state.m_pipelineCache;
        init_info.DescriptorPool = m_descriptorPool;
        init_info.RenderPass = sdlwindow->m_mainWindowData.RenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = sdlwindow->m_minImageCount;
        init_info.ImageCount = sdlwindow->m_mainWindowData.ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = state.m_allocator;
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
            WindowSDL<ATYPE>* sdlwindow = (WindowSDL<ATYPE>*)m_window;
            sdlwindow->m_mainWindowData.ClearValue.color.float32[0] = sdlwindow->m_clearColor.x * sdlwindow->m_clearColor.w;
            sdlwindow->m_mainWindowData.ClearValue.color.float32[1] = sdlwindow->m_clearColor.y * sdlwindow->m_clearColor.w;
            sdlwindow->m_mainWindowData.ClearValue.color.float32[2] = sdlwindow->m_clearColor.z * sdlwindow->m_clearColor.w;
            sdlwindow->m_mainWindowData.ClearValue.color.float32[3] = sdlwindow->m_clearColor.w;
            
            //FrameRender(&sdlwindow->m_mainWindowData, draw_data);
            auto wd = &sdlwindow->m_mainWindowData;

            VkResult err;

            auto& state = m_engine->GetState();
            //WindowSDL<ATYPE>* sdlwindow = (WindowSDL<ATYPE>*)m_window;

            VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
            VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
            err = vkAcquireNextImageKHR(state.m_device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            {
                sdlwindow->m_swapChainRebuild = true;
                return;
            }
            vh::CheckResult(err);

            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
            {
                vh::CheckResult(vkWaitForFences(state.m_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX));
                vh::CheckResult(vkResetFences(state.m_device, 1, &fd->Fence));
            }
            {
                vh::CheckResult(vkResetCommandPool(state.m_device, fd->CommandPool, 0));
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
                vh::CheckResult(vkQueueSubmit(state.m_queue, 1, &info, fd->Fence));
            }
        }
    }


   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnQuit(Message message) {}

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

