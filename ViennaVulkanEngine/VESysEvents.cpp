/**
*
* \file
* \brief
*
* Details
*
*/


#include "VEDefines.h"
#include "VESysEngine.h"
#include "VESysEvents.h"


namespace vve::syseve {

	//--------------------------------------------------------------------------------------------------
	std::vector<VeMap*> maps2 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)),

	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table(maps2, true, true, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);

	std::vector<VeMap*> maps3 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)),

		new VeTypedMap< std::map<VeTableKeyIntTriple, VeTableIndex>, VeTableKeyIntTriple, VeTableIndexTriple >
		(VeTableIndexTriple{(VeIndex)offsetof(VeEventTableEntry, m_type), (VeIndex)offsetof(VeEventTableEntry, m_key_button), (VeIndex)offsetof(VeEventTableEntry, m_action)},
		 VeTableIndexTriple{(VeIndex)sizeof(VeEventTableEntry::m_type),   (VeIndex)sizeof(VeEventTableEntry::m_key_button),   (VeIndex)sizeof(VeEventTableEntry::m_action)})
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_continuous_events_table(maps3, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_continuous_events_table2(g_continuous_events_table);

	//--------------------------------------------------------------------------------------------------
	struct VeEventHandlerTableEntry {
		std::function<void(VeEventTableEntry)> m_handler;
	};
	VeFixedSizeTableMT<VeEventHandlerTableEntry> g_handler_table(false, false, 0, 0);
	VeFixedSizeTableMT<VeEventHandlerTableEntry> g_handler_table2(g_handler_table);

	//--------------------------------------------------------------------------------------------------
	struct VeEventSubscribeTableEntry {
		VeEventType	m_type;
		VeHandle	m_handlerH;
	};
	std::vector<VeMap*> maps4 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
			(offsetof(VeEventSubscribeTableEntry, m_type), sizeof(VeEventSubscribeTableEntry::m_type)),
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
			(offsetof(VeEventSubscribeTableEntry, m_handlerH), sizeof(VeEventSubscribeTableEntry::m_handlerH)),
		new VeTypedMap< std::map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >
			(VeTableIndexPair{(VeIndex)offsetof(VeEventSubscribeTableEntry, m_type), (VeIndex)offsetof(VeEventSubscribeTableEntry, m_handlerH)},
			 VeTableIndexPair{(VeIndex)sizeof(VeEventSubscribeTableEntry::m_type),   (VeIndex)sizeof(VeEventSubscribeTableEntry::m_handlerH)})
	};
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table(maps4, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table2(g_subscribe_table);

	void init() {
		syseng::registerTablePointer(&g_events_table, "Events Table");
		syseng::registerTablePointer(&g_handler_table, "Event Handler Table");
		syseng::registerTablePointer(&g_subscribe_table, "Event Subscribe Table");
	}

	void callEvents2(VeFixedSizeTable<VeEventTableEntry>& events_table) {
		std::vector<VeTableHandlePair> result;
		events_table.leftJoin<std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex>(0, g_subscribe_table, 0, result);
		for( auto [eventhandle, subscribehandle] : result ) {
			VeEventTableEntry eventData;
			events_table.getEntry(eventhandle, eventData);
			VeEventSubscribeTableEntry subscribeData;
			g_subscribe_table.getEntry(subscribehandle, subscribeData);
			VeEventHandlerTableEntry handlerData;
			g_handler_table.getEntry(subscribeData.m_handlerH, handlerData);
			JADD( handlerData.m_handler(eventData) );
		}
	}


	void callEvents( VeEventType type, VeIndex action ) {
		//TODO order event types here!!!
	}


	void tick() {
		callEvents2(g_events_table);
		callEvents2(g_continuous_events_table);
	}

	void cleanUp() {
	}

	void close() {
	}

	void addEvent(VeEventTableEntry event) {
		//std::cout << "add event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		g_events_table.addEntry( event );
	}

	void addContinuousEvent(VeEventTableEntry event) {
		std::cout << "add cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		g_continuous_events_table.addEntry(event);
	}

	void removeContinuousEvent(VeEventTableEntry event) {
		std::cout << "remove cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		VeHandle eventH = g_continuous_events_table.getHandleEqual(1, VeTableKeyIntTriple{ event.m_type, event.m_key_button, event.m_action });
		assert(eventH != VE_NULL_HANDLE);
		g_continuous_events_table.deleteEntry(eventH);
	}

	void addHandler(std::function<void(VeEventTableEntry)> handler, VeHandle *pHandle) {
		g_handler_table.addEntry({handler}, pHandle);
	}

	void removeHandler(VeHandle handlerH) {
		std::vector<VeHandle> result;
		g_subscribe_table.getHandlesEqual(1, handlerH, result);
		for (auto handle : result) {
			g_subscribe_table.deleteEntry(handle);
		}
		g_handler_table.deleteEntry(handlerH);
	}

	void subscribeEvent(VeEventType type, VeHandle handlerH) {
		g_subscribe_table.addEntry({type, handlerH});
	}

	void unsubscribeEvent(VeHandle typeH, VeHandle handlerH) {
		VeHandle subH = g_subscribe_table.getHandleEqual(2, VeTableKeyIntPair{ typeH, handlerH });
		g_subscribe_table.deleteEntry(subH);
	}

}

