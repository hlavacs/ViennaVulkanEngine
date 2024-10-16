
#include "VEWindowSDL.h"
#include "VEEngine.h"

namespace vve {

    VeWindowSDL::VeWindowSDL(VeEngine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions) 
                : VeWindow(engine, instance, windowName, width, height, instance_extensions) {

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

    VeWindowSDL::~VeWindowSDL() {
        vh::CheckResult(vkDeviceWaitIdle(m_engine.getDevice()));
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        ImGui_ImplVulkanH_DestroyWindow(m_engine.getInstance(), m_engine.getDevice(), &m_mainWindowData, m_engine.getAllocator());
        vkDestroyDescriptorPool(m_engine.getDevice(), m_descriptorPool, m_engine.getAllocator());
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }


    void VeWindowSDL::Init() {
        if (SDL_Vulkan_CreateSurface(m_window, m_engine.getInstance(), &m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }
        
        vh::SetupDescriptorPool(m_engine.getDevice(), &m_descriptorPool);

        // Setup Vulkan
        // Create Framebuffers
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);

         m_mainWindowData.Surface = m_surface;

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_engine.getPhysicalDevice(), m_engine.getQueueFamily(), m_mainWindowData.Surface, &res);
        if (res != VK_TRUE) {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        m_mainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(m_engine.getPhysicalDevice(), m_mainWindowData.Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

        // Select Present Mode
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
        m_mainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(m_engine.getPhysicalDevice(), m_mainWindowData.Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
        //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

        // Create SwapChain, RenderPass, Framebuffer, etc.
        assert(m_minImageCount >= 2);
        ImGui_ImplVulkanH_CreateOrResizeWindow(m_engine.getInstance(), m_engine.getPhysicalDevice(), m_engine.getDevice(), &m_mainWindowData, m_engine.getQueueFamily(), m_engine.getAllocator(), w, h, m_minImageCount);

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
        ImGui_ImplSDL2_InitForVulkan(m_window);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_engine.getInstance();
        init_info.PhysicalDevice = m_engine.getPhysicalDevice();
        init_info.Device = m_engine.getDevice();
        init_info.QueueFamily = m_engine.getQueueFamily();
        init_info.Queue = m_engine.getQueue();
        init_info.PipelineCache = m_engine.getPipelineCache();
        init_info.DescriptorPool = m_descriptorPool;
        init_info.RenderPass = m_mainWindowData.RenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = m_minImageCount;
        init_info.ImageCount = m_mainWindowData.ImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = m_engine.getAllocator();
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


    bool VeWindowSDL::pollEvents() {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                m_engine.Stop();
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(m_window))
                m_engine.Stop();
        }
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            return false;
        }

        // Resize swap chain?
        int fb_width, fb_height;
        SDL_GetWindowSize(m_window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (m_swapChainRebuild || m_mainWindowData.Width != fb_width || m_mainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(m_engine.getInstance(), m_engine.getPhysicalDevice(), m_engine.getDevice(), &m_mainWindowData, m_engine.getQueueFamily(), m_engine.getAllocator(), fb_width, fb_height, m_minImageCount);
            m_mainWindowData.FrameIndex = 0;
            m_swapChainRebuild = false;
        }

        return true;
    }


    void VeWindowSDL::renderNextFrame() {
        // Rendering

        static bool show_demo_window = true;
        static bool show_another_window = false;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / m_io->Framerate, m_io->Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            m_mainWindowData.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            m_mainWindowData.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            m_mainWindowData.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            m_mainWindowData.ClearValue.color.float32[3] = clear_color.w;
            FrameRender(&m_mainWindowData, draw_data);
            FramePresent(&m_mainWindowData);
        }
    }


    void VeWindowSDL::FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
    {
        VkResult err;

        VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        err = vkAcquireNextImageKHR(m_engine.getDevice(), wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            m_swapChainRebuild = true;
            return;
        }
        vh::CheckResult(err);

        ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
        {
            vh::CheckResult(vkWaitForFences(m_engine.getDevice(), 1, &fd->Fence, VK_TRUE, UINT64_MAX));
            vh::CheckResult(vkResetFences(m_engine.getDevice(), 1, &fd->Fence));
        }
        {
            vh::CheckResult(vkResetCommandPool(m_engine.getDevice(), fd->CommandPool, 0));
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
            vh::CheckResult(vkQueueSubmit(m_engine.getQueue(), 1, &info, fd->Fence));
        }
    }

    void VeWindowSDL::FramePresent(ImGui_ImplVulkanH_Window* wd)
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
        VkResult err = vkQueuePresentKHR(m_engine.getQueue(), &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            m_swapChainRebuild = true;
            return;
        }
        vh::CheckResult(err);
        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
    }




    std::pair<int, int> VeWindowSDL::getSize() {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return std::make_pair(w, h);
    }

    bool VeWindowSDL::InitSDL(VkInstance instance) {
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





};  // namespace vve
