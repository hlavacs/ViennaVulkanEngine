

#include "VEDefines.h"
#include "vulkan.h"
#include "VESysEngine.h"
#include "VESysVulkan.h"


namespace vve::sysvul {


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
	VeFixedSizeTable<VeRendererTableEntry> g_renderer_table;


	struct VePhysicalDeviceTableEntry {
		VkPhysicalDevice m_physicalDevice;

	};
	VeFixedSizeTable<VePhysicalDeviceTableEntry> g_physical_device_table;

	VkDevice g_device;

	struct VeQueueFamilyTableEntry {
		VeHandle				m_physicalDeviceH;
		VkQueueFamilyProperties m_familyProperties;
	};
	VeFixedSizeTable<VkQueueFamilyProperties> g_queue_family_table;


	struct VeQueueTableEntry {
		VkQueue		m_queue;
		VeHandle	m_queueFamilyH;
	};
	VeFixedSizeTable<VeQueueTableEntry> g_queue_table;


	void createTables() {
		syseng::registerTablePointer(&g_renderer_table, "Renderer");
		syseng::registerTablePointer(&g_physical_device_table, "Physical Devices");
		syseng::registerTablePointer(&g_queue_family_table, "Queue Families");
		syseng::registerTablePointer(&g_queue_table, "Queues");
	}

	void init() {
		createTables();



	}

	void tick() {

	}

	void sync() {
	}

	void close() {

	}


}


