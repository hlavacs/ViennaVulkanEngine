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
	struct VeEventTypeTableEntry {
		VeIndex m_type;
	};
	std::vector<VeMap*> maps1 = {
		new VeTypedMap< std::unordered_map<VeHandle, VeIndex>, VeHandle, VeIndex >
		(offsetof(VeEventTypeTableEntry, m_type), sizeof(VeEventTypeTableEntry::m_type))
	};
	VeFixedSizeTable<VeEventTypeTableEntry> g_event_types_table(maps1, true, false, 0, 0);

	//--------------------------------------------------------------------------------------------------
	std::vector<VeMap*> maps2 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_typeH), sizeof(VeEventTableEntry::m_typeH))
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table(maps2, true, true, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);

	std::vector<VeMap*> maps3 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_typeH), sizeof(VeEventTableEntry::m_typeH)),

		new VeTypedMap< std::map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >
		(VeTableIndexPair{(VeIndex)offsetof(VeEventTableEntry, m_typeH), (VeIndex)offsetof(VeEventTableEntry, m_key_button)},
		 VeTableIndexPair{(VeIndex)sizeof(VeEventTableEntry::m_typeH),    (VeIndex)sizeof(VeEventTableEntry::m_key_button)})
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
		VeHandle m_typeH;
		VeHandle m_handlerH;
	};
	std::vector<VeMap*> maps4 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
			(offsetof(VeEventSubscribeTableEntry, m_typeH), sizeof(VeEventSubscribeTableEntry::m_typeH)),
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
			(offsetof(VeEventSubscribeTableEntry, m_handlerH), sizeof(VeEventSubscribeTableEntry::m_handlerH)),
		new VeTypedMap< std::map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >
			(VeTableIndexPair{(VeIndex)offsetof(VeEventSubscribeTableEntry, m_typeH), (VeIndex)offsetof(VeEventSubscribeTableEntry, m_handlerH)},
			 VeTableIndexPair{(VeIndex)sizeof(VeEventSubscribeTableEntry::m_typeH),   (VeIndex)sizeof(VeEventSubscribeTableEntry::m_handlerH)})
	};
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table(maps4, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table2(g_subscribe_table);

	void init() {
		for (uint32_t i = VE_EVENT_TYPE_NULL; i <= VE_EVENT_TYPE_LAST; ++i) {
			g_event_types_table.addEntry({i});
		}
		g_event_types_table.setReadOnly(true);
		syseng::registerTablePointer(&g_event_types_table, "Events Types Table");
		syseng::registerTablePointer(&g_events_table, "Events Table");
		syseng::registerTablePointer(&g_handler_table, "Event Handler Table");
		syseng::registerTablePointer(&g_subscribe_table, "Event Subscribe Table");
	}


	void callEvents(VeFixedSizeTable<VeEventTableEntry>& events_table) {
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

	void tick() {
		callEvents(g_events_table);
		callEvents(g_continuous_events_table);
	}

	void cleanUp() {
	}

	void close() {
	}

	void addEvent(VeEventType type, VeEventTableEntry event) {
		std::cout << "add event type " << type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		event.m_typeH = g_event_types_table.getHandleEqual(0, type);	//table is read only
		g_events_table.addEntry( event );
	}

	void addContinuousEvent(VeEventType type, VeEventTableEntry event) {
		std::cout << "add cont event type " << type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		event.m_typeH = g_event_types_table.getHandleEqual(0, type);	//table is read only
		g_continuous_events_table.addEntry(event);
	}

	void removeContinuousEvent(VeEventType type, VeEventTableEntry event) {
		std::cout << "remove cont event type " << type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		event.m_typeH = g_event_types_table.getHandleEqual(0, type);	//table is read only
		VeHandle eventH = g_continuous_events_table.getHandleEqual(1, VeTableKeyIntPair{ event.m_typeH, event.m_key_button });
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
		VeHandle typeH = g_event_types_table.getHandleEqual(0, type);	//table is read only
		g_subscribe_table.addEntry({typeH, handlerH});
	}

	void unsubscribeEvent(VeHandle typeH, VeHandle handlerH) {
		VeHandle subH = g_subscribe_table.getHandleEqual(2, VeTableKeyIntPair{ typeH, handlerH });
		g_subscribe_table.deleteEntry(subH);
	}

}

