
#include "VEWindowSDL.h"
#include "VEEngine.h"
#include "VHImgui.h"

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

    VeWindowSDL::~VeWindowSDL(){
        //ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
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
        //init_info.PipelineCache = g_PipelineCache;
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


    void VeWindowSDL::pollEvents() {

    }


    void VeWindowSDL::renderNextFrame() {
        imgui_SDL2( m_engine.getInstance(), m_engine.getPhysicalDevice(), m_engine.getDevice()
            , m_engine.getQueue(), m_engine.getQueueFamily(), m_surface
            , m_descriptorPool, m_engine.getAllocator(), m_window, &m_mainWindowData, m_io);
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
