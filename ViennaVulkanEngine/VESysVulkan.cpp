

#include "vulkan.h"

#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"


namespace vve::sysvul {


	VkInstance g_instance = VK_NULL_HANDLE;
	VkDevice g_device = VK_NULL_HANDLE;

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

	struct VePhysicalDeviceTableEntry {
		VkPhysicalDevice m_physicalDevice;
	};
	VeFixedSizeTable<VePhysicalDeviceTableEntry> g_physical_devices_table;

	struct VeQueueFamilyTableEntry {
		VeHandle				m_physicalDeviceH;
		VkQueueFamilyProperties m_familyProperties;
	};
	VeFixedSizeTable<VkQueueFamilyProperties> g_queue_families_table;

	struct VeQueueTableEntry {
		VkQueue		m_queue;
		VeHandle	m_queueFamilyH;
	};
	VeFixedSizeTable<VeQueueTableEntry> g_queues_table;


	VeFixedSizeTable<VeStringTableEntry> g_instance_extensions_table;
	VeFixedSizeTable<VeStringTableEntry> g_instance_layers_table;
	VeFixedSizeTable<VeStringTableEntry> g_device_extensions_table;

	void createTables() {
		syseng::registerTablePointer(&g_renderers_table, "Renderer");
		syseng::registerTablePointer(&g_physical_devices_table, "Physical Devices");
		syseng::registerTablePointer(&g_queue_families_table, "Queue Families");
		syseng::registerTablePointer(&g_queues_table, "Queues");
		syseng::registerTablePointer(&g_instance_extensions_table, "Instance Extensions");
		syseng::registerTablePointer(&g_instance_layers_table, "Instance Layers");
		syseng::registerTablePointer(&g_device_extensions_table, "Device Extensions");
	}

	void init() {
		createTables();

		g_instance_extensions_table.addEntry({ "VK_EXT_debug_report" });
		g_instance_layers_table.addEntry({ "VK_LAYER_KHRONOS_validation" });
		g_instance_layers_table.addEntry({ "VK_LAYER_LUNARG_monitor" });
		g_device_extensions_table.addEntry({ "VK_KHR_swapchain" });
		
	}

	void tick() {

	}

	void sync() {
	}

	void close() {

	}


}


