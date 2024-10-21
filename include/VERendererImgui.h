#pragma once

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using Renderer<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(std::string name, Engine<ATYPE>& engine, std::weak_ptr<Window<ATYPE>> window);
        virtual ~RendererImgui();

    private:
        virtual void PrepareRender() override;
        virtual void Render() override;

        ImGuiIO* m_io;
   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	
    };

};   // namespace vve
