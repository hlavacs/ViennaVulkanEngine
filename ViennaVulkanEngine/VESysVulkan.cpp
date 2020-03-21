/**
*
* \file
* \brief
*
* Details
*
*/

#include "vulkan/vulkan.h"

#include "VEDefines.h"
#include "VHInclude.h"
#include "VHDevice.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"


namespace vve::sysvul {


	vh::VhVulkanState g_vulkan_state;

	struct VeRendererTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_close;
		VeRendererTableEntry() : m_init(), m_tick(), m_close() {};
		VeRendererTableEntry(std::function<void()> init, std::function<void()> tick,	std::function<void()> close) :
			m_init(init), m_tick(tick), m_close(close) {};
	};
	VeFixedSizeTable<VeRendererTableEntry> g_renderers_table("Renderer Table", false, false, 0, 0 );

	void registerRenderer(std::function<void()> init, std::function<void()> tick, std::function<void()> close) {
		g_renderers_table.addEntry({ init, tick, close });
	}


	void init() {
		std::vector<const char*> extensions = { "VK_EXT_debug_report" };
		std::vector<const char*> layers = {
			"VK_LAYER_KHRONOS_validation",  "VK_LAYER_LUNARG_monitor"
		};

		vh::dev::vhCreateInstance(g_vulkan_state, extensions, layers );
	}

	bool g_windowSizeChanged = false;
	VeClock tickClock("Vulkan Clock");

	void update() {
		//tickClock.tick();

	}

	void cleanUp() {
		if (g_windowSizeChanged) {
			g_windowSizeChanged = false;
			//recreate swap chain, do not draw current frame
		}

		if (syseng::getNowTime() > syseng::getNextUpdateTime())	//have not reached the now time interval
			return;

	}

	void close() {
		vkDestroyInstance(g_vulkan_state.m_instance, nullptr );
	}


	void windowSizeChanged() {
		g_windowSizeChanged = true;
	}


}


