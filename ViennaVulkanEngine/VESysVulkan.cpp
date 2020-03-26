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
#include "VESysEvents.h"
#include "VESysEngine.h"
#include "VESysWindow.h"
#include "VESysVulkan.h"
#include "VESysVulkanFWSimple.h"


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
		g_renderers_table.insert({ init, tick, close });
	}

	VeHandle g_updateHandle;
	VeHandle g_closeHandle;

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		g_updateHandle = syseve::addHandler(std::bind(update, std::placeholders::_1));
		syseve::subscribeEvent(	syseng::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_updateHandle,
								syseve::VeEventType::VE_EVENT_TYPE_UPDATE);

		g_closeHandle = syseve::addHandler(std::bind(close, std::placeholders::_1));
		syseve::subscribeEvent(	syswin::VE_SYSTEM_HANDLE, VE_NULL_HANDLE, g_closeHandle,
								syseve::VeEventType::VE_EVENT_TYPE_CLOSE);

		std::vector<const char*> extensions = { "VK_EXT_debug_report" };
		std::vector<const char*> layers = {
			"VK_LAYER_KHRONOS_validation",  "VK_LAYER_LUNARG_monitor"
		};

		vh::dev::vhCreateInstance(g_vulkan_state, extensions, layers );
	}

	bool g_windowSizeChanged = false;
	VeClock tickClock("Vulkan Clock");

	void update(syseve::VeEventTableEntry e) {
		//tickClock.tick();
		fwsimple::update(e);
	}

	void cleanUp() {
		if (g_windowSizeChanged) {
			g_windowSizeChanged = false;
			//recreate swap chain, do not draw current frame
		}

		if (syseng::getNowTime() > syseng::getNextUpdateTime())	//have not reached the now time interval
			return;

	}

	void close(syseve::VeEventTableEntry e) {
		fwsimple::close(e);
		vkDestroyInstance(g_vulkan_state.m_instance, nullptr );
	}


	void windowSizeChanged() {
		g_windowSizeChanged = true;
	}


}


