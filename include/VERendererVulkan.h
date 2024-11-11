#pragma once

#include <any>
#include "VHInclude.h"
#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:

        RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererVulkan" );
        virtual ~RendererVulkan();
        auto GetInstance() -> VkInstance { return m_instance; }

    private:
        virtual void OnInit(Message message);
        virtual void OnInit2(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnQuit(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        WindowSDL<ATYPE> *m_windowSDL;
        VkInstance m_instance;
    };
};   // namespace vve

