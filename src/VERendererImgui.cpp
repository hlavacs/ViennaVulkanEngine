
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
    RendererImgui<ATYPE>::RendererImgui(std::string name, Engine<ATYPE>& engine, std::weak_ptr<Window<ATYPE>> window) 
        : Renderer<ATYPE>(name, engine, window) {

        WindowSDL<ATYPE>* sdlwindow = (WindowSDL<ATYPE>*)(m_window.lock().get());
        auto state = m_engine.GetState();
        vh::CreateWindowCommandBuffers(state.m_physicalDevice, state.m_device, &sdlwindow->m_mainWindowData, state.m_queueFamily, state.m_allocator);
        vh::SetupDescriptorPool(m_engine.GetState().m_device, &m_descriptorPool);

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
    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui(){

        vkDestroyDescriptorPool(m_engine.GetState().m_device, m_descriptorPool, m_engine.GetState().m_allocator);
    };

    template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::PrepareRender() {};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::Render(){
    };

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

