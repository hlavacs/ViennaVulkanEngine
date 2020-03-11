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

		new VeTypedMap< std::unordered_multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)), 

		new VeTypedMap< std::multimap<VeTableKeyIntTriple, VeTableIndex>, VeTableKeyIntTriple, VeTableIndexTriple >
		(VeTableIndexTriple{(VeIndex)offsetof(VeEventTableEntry, m_type), (VeIndex)offsetof(VeEventTableEntry, m_key_button), (VeIndex)offsetof(VeEventTableEntry, m_action)},
		 VeTableIndexTriple{(VeIndex)sizeof(VeEventTableEntry::m_type),   (VeIndex)sizeof(VeEventTableEntry::m_key_button),   (VeIndex)sizeof(VeEventTableEntry::m_action)})

	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table(maps2, true, true, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);

	std::vector<VeMap*> maps3 = {
		new VeTypedMap< std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)),

		new VeTypedMap< std::unordered_multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
		(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)), 

		new VeTypedMap< std::multimap<VeTableKeyIntTriple, VeTableIndex>, VeTableKeyIntTriple, VeTableIndexTriple >
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

		new VeTypedMap< std::unordered_multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex >
			(offsetof(VeEventSubscribeTableEntry, m_handlerH), sizeof(VeEventSubscribeTableEntry::m_handlerH)),

		new VeTypedMap< std::unordered_map<VeTableKeyIntPair, VeTableIndex>, VeTableKeyIntPair, VeTableIndexPair >
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

	void callAllEvents(VeFixedSizeTable<VeEventTableEntry>& events_table) {
		std::vector<VeTableHandlePair> result;
		VeEventTableEntry eventData;
		VeEventSubscribeTableEntry subscribeData;
		VeEventHandlerTableEntry handlerData;

		events_table.leftJoin<std::multimap<VeTableKeyInt, VeTableIndex>, VeTableKeyInt, VeTableIndex>(0, g_subscribe_table, 0, result);
		for( auto [eventhandle, subscribehandle] : result ) {
			events_table.getEntry(eventhandle, eventData);
			g_subscribe_table.getEntry(subscribehandle, subscribeData);
			g_handler_table.getEntry(subscribeData.m_handlerH, handlerData);
			JADD( handlerData.m_handler(eventData) );
		}
	}


	void callEventsKey(VeFixedSizeTableMT<VeEventTableEntry>* table, VeEventType type, VeIndex action) {
		VeEventTableEntry eventData;
		VeEventSubscribeTableEntry subscriptionData;
		VeEventHandlerTableEntry handlerData;

		std::vector<VeHandle, custom_alloc<VeHandle>> events(table->getAllocator().m_handle);
		std::vector<VeHandle, custom_alloc<VeHandle>> subscriptions(table->getAllocator().m_handle);
		table->getHandlesEqual(1, type, events);
		for (auto evH : events) {
			g_subscribe_table.getHandlesEqual(0, evH, subscriptions);
			for (auto subH : subscriptions) {
				g_subscribe_table.getEntry(subH, subscriptionData);

			}
		}
	}

	void callEvents(VeFixedSizeTableMT<VeEventTableEntry> *table, VeEventType type, VeIndex action) {
		JADD(callEventsKey( table, type, action));
		//JDEP(callEventsInOrder( table, (VeEventType)(type + 1), action ));
	}

	void callEventsInOrder(VeFixedSizeTableMT<VeEventTableEntry> *table, VeEventType type, VeIndex action ) {
		callEvents(table, type, action);
		VeEventType nt = (VeEventType)(type + 1);
		//if( type == )
	}


	void tick() {
		return;

		JADD( callEventsInOrder( &g_events_table, VE_EVENT_TYPE_LOOP_TICK, VE_NULL_INDEX ) );

		callAllEvents(g_continuous_events_table);
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
		std::vector<VeHandle, custom_alloc<VeHandle>> result(g_subscribe_table.getAllocator().m_handle);
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

