

#include "vulkan.h"

#include "VEDefines.h"
#include "VHDevice.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"


namespace vve::sysvul {


	VeVulkanState g_vulkan_state;

	struct VeRendererTableEntry {
		std::function<void()>	m_init;
		std::function<void()>	m_tick;
		std::function<void()>	m_sync;
		std::function<void()>	m_close;
		VeRendererTableEntry() : m_init(), m_tick(), m_sync(), m_close() {};
		VeRendererTableEntry(std::function<void()> init, std::function<void()> tick,
			std::function<void()> sync, std::function<void()> close) :
			m_init(init), m_tick(tick), m_sync(sync), m_close(close) {};
	};
	VeFixedSizeTable<VeRendererTableEntry> g_renderers_table;


	void init() {
		std::vector<const char*> extensions = { "VK_EXT_debug_report" };
		std::vector<const char*> layers = {
			"VK_LAYER_KHRONOS_validation",  "VK_LAYER_LUNARG_monitor"
		};

		dev::vhCreateInstance(extensions, layers, &g_vulkan_state.m_instance);

		
	}

	void tick() {

	}

	void sync() {
	}

	void close() {

		vkDestroyInstance(g_vulkan_state.m_instance, nullptr );

	}


}


