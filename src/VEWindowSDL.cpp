
#include "VESystem.h"
#include "VHDevice.h"
#include "VEWindowSDL.h"
#include "VEEngine.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::WindowSDL(Engine<ATYPE>& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions) 
                : Window<ATYPE>(engine, instance, windowName, width, height, instance_extensions) {

        if(!sdl_initialized) {
            sdl_initialized = InitSDL(instance);
            if(!sdl_initialized)  return;
        }

        // Create window with Vulkan graphics context
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        m_window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
        if (m_window == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            return;
        }

        uint32_t extensions_count = 0;
        std::vector<const char*> extensions;
        SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, nullptr);
        extensions.resize(extensions_count);
        SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, extensions.data());
        instance_extensions.insert(instance_extensions.end(), extensions.begin(), extensions.end());
        
    }

   	template<ArchitectureType ATYPE>
    WindowSDL<ATYPE>::~WindowSDL() {
        vh::CheckResult(vkDeviceWaitIdle(m_engine.GetState().m_device));
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        ImGui_ImplVulkanH_DestroyWindow(m_engine.GetState().m_instance, m_engine.GetState().m_device, &m_mainWindowData, m_engine.GetState().m_allocator);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::Init() {
        if (SDL_Vulkan_CreateSurface(m_window, m_engine.GetState().m_instance, &m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }
        
        // Setup Vulkan
        // Create Framebuffers
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);

        m_mainWindowData.Surface = m_surface;

        auto& state = m_engine.GetState();

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(state.m_physicalDevice, state.m_queueFamily, m_mainWindowData.Surface, &res);
        if (res != VK_TRUE) {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }

        // Select Surface Format
        std::vector<VkFormat> requestSurfaceFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        m_mainWindowData.SurfaceFormat = vh::SelectSurfaceFormat(state.m_physicalDevice, m_mainWindowData.Surface, requestSurfaceFormats);

        // Select Present Mode
        std::vector<VkPresentModeKHR> requestedPresentModes = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
        m_mainWindowData.PresentMode = vh::SelectPresentMode(state.m_physicalDevice, m_mainWindowData.Surface, requestedPresentModes);

        vh::CreateWindowSwapChain(state.m_physicalDevice, state.m_device, &m_mainWindowData, state.m_allocator, w, h, m_minImageCount);
    }


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnPollEvents(Message message) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

        static std::vector<SDL_Scancode> key;
        static std::vector<int8_t> button;
        key.clear();
        button.clear();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                m_engine.Stop();
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(m_window))
                m_engine.Stop();

            switch( event.type ) {
                case SDL_MOUSEMOTION:
                    m_engine.SendMessage( MessageMouseMove{event.motion.x, event.motion.y} );
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    m_engine.SendMessage( MessageMouseButtonDown{event.button.button} );
                    button.push_back( event.button.button );
                    break;
                case SDL_MOUSEBUTTONUP:
                    m_engine.SendMessage( MessageMouseButtonUp{event.button.button} );
                    m_mouseButtonsDown.erase( event.button.button );
                    break;
                case SDL_MOUSEWHEEL:
                    m_engine.SendMessage( MessageMouseWheel{event.wheel.x, event.wheel.y} );
                    break;
                case SDL_KEYDOWN:
                    if( event.key.repeat ) continue;
                    m_engine.SendMessage( MessageKeyDown{event.key.keysym.scancode} );
                    key.push_back(event.key.keysym.scancode);
                    break;
                case SDL_KEYUP:
                    m_engine.SendMessage( MessageKeyUp{event.key.keysym.scancode} );
                    m_keysDown.erase(event.key.keysym.scancode);
                    break;
                default:
                    break;
            }
        }

        for( auto& key : m_keysDown ) { m_engine.SendMessage( MessageKeyRepeat{key} ); }
        for( auto& button : m_mouseButtonsDown ) { m_engine.SendMessage( MessageMouseButtonRepeat{button} ); }

        if(key.size() > 0) { for( auto& k : key ) { m_keysDown.insert(k) ; } }
        if(button.size() > 0) { for( auto& b : button ) {m_mouseButtonsDown.insert(b);} }

        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            return;
        }

        // Resize swap chain?
        int fb_width, fb_height;
        SDL_GetWindowSize(m_window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (m_swapChainRebuild || m_mainWindowData.Width != fb_width || m_mainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(m_engine.GetState().m_instance, m_engine.GetState().m_physicalDevice, m_engine.GetState().m_device, &m_mainWindowData, m_engine.GetState().m_queueFamily, m_engine.GetState().m_allocator, fb_width, fb_height, m_minImageCount);
            m_mainWindowData.FrameIndex = 0;
            m_swapChainRebuild = false;
        }

        return ;
    }


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnPrepareNextFrame(Message message) {
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::OnRenderNextFrame(Message message) {
        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            m_mainWindowData.ClearValue.color.float32[0] = m_clearColor.x * m_clearColor.w;
            m_mainWindowData.ClearValue.color.float32[1] = m_clearColor.y * m_clearColor.w;
            m_mainWindowData.ClearValue.color.float32[2] = m_clearColor.z * m_clearColor.w;
            m_mainWindowData.ClearValue.color.float32[3] = m_clearColor.w;
            FrameRender(&m_mainWindowData, draw_data);
            FramePresent(&m_mainWindowData);
        }
    }


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
    {
        VkResult err;

        auto& state = m_engine.GetState();

        VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        err = vkAcquireNextImageKHR(state.m_device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            m_swapChainRebuild = true;
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
        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

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


   	template<ArchitectureType ATYPE>
    void WindowSDL<ATYPE>::FramePresent(ImGui_ImplVulkanH_Window* wd)
    {
        if (m_swapChainRebuild)
            return;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &wd->Swapchain;
        info.pImageIndices = &wd->FrameIndex;
        VkResult err = vkQueuePresentKHR(m_engine.GetState().m_queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            m_swapChainRebuild = true;
            return;
        }
        vh::CheckResult(err);
        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
    }


   	template<ArchitectureType ATYPE>
    auto WindowSDL<ATYPE>::GetSize() -> std::pair<int, int> {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return std::make_pair(w, h);
    }

   	template<ArchitectureType ATYPE>
    bool WindowSDL<ATYPE>::InitSDL(VkInstance instance) {
        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            printf("Error: %s\n", SDL_GetError());
            return false;
        }

        // From 2.0.18: Enable native IME.
    #ifdef SDL_HINT_IME_SHOW_UI
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    #endif

        return true;
    }

    template class WindowSDL<ArchitectureType::SEQUENTIAL>;
    template class WindowSDL<ArchitectureType::PARALLEL>;

};  // namespace vve
