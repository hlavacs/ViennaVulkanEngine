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
	//events 

	std::vector<VeMap*> maps1 = {
		new VeOrderedMultimap< VeHandle, VeIndex >(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)),
		new VeOrderedMultimap< VeHandleTriple, VeIndexTriple >
		(VeIndexTriple{(VeIndex)offsetof(VeEventTableEntry, m_type), (VeIndex)offsetof(VeEventTableEntry, m_key_button), (VeIndex)offsetof(VeEventTableEntry, m_action)},
		 VeIndexTriple{(VeIndex)sizeof(VeEventTableEntry::m_type),   (VeIndex)sizeof(VeEventTableEntry::m_key_button),   (VeIndex)sizeof(VeEventTableEntry::m_action)})
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table( "Events Table", maps1, true, true, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);


	std::vector<VeMap*> maps2 = {
		new VeOrderedMultimap< VeHandle, VeIndex >(offsetof(VeEventTableEntry, m_type), sizeof(VeEventTableEntry::m_type)),
		new VeOrderedMultimap< VeHandleTriple, VeIndexTriple >
		(VeIndexTriple{(VeIndex)offsetof(VeEventTableEntry, m_type), (VeIndex)offsetof(VeEventTableEntry, m_key_button), (VeIndex)offsetof(VeEventTableEntry, m_action)},
		 VeIndexTriple{(VeIndex)sizeof(VeEventTableEntry::m_type),   (VeIndex)sizeof(VeEventTableEntry::m_key_button),   (VeIndex)sizeof(VeEventTableEntry::m_action)})
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_continuous_events_table("Continuous Events Table", maps2, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_continuous_events_table2(g_continuous_events_table);


	//--------------------------------------------------------------------------------------------------
	//event handler

	struct VeEventHandlerTableEntry {
		std::function<void(VeEventTableEntry)> m_handler;
	};

	VeFixedSizeTableMT<VeEventHandlerTableEntry> g_handler_table("Event Handler Table", false, false, 0, 0);
	VeFixedSizeTableMT<VeEventHandlerTableEntry> g_handler_table2(g_handler_table);


	//--------------------------------------------------------------------------------------------------
	//subscribers

	struct VeEventSubscribeTableEntry {
		VeHandle	m_senderH;
		VeHandle	m_handlerH;
	};

	std::vector<VeMap*> maps4 = {
		new VeOrderedMultimap< VeHandle, VeIndex >(offsetof(VeEventSubscribeTableEntry, m_senderH), sizeof(VeEventSubscribeTableEntry::m_senderH)),
		new VeOrderedMultimap< VeHandle, VeIndex >(offsetof(VeEventSubscribeTableEntry, m_handlerH), sizeof(VeEventSubscribeTableEntry::m_handlerH)),
		new VeOrderedMultimap< VeHandlePair, VeIndexPair >
			(VeIndexPair{(VeIndex)offsetof(VeEventSubscribeTableEntry, m_senderH), (VeIndex)offsetof(VeEventSubscribeTableEntry, m_senderH)},
			 VeIndexPair{(VeIndex)sizeof(VeEventSubscribeTableEntry::m_senderH),   (VeIndex)sizeof(VeEventSubscribeTableEntry::m_handlerH)})
	};
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table("Event Subscribe Table", maps4, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table2(g_subscribe_table);


	//--------------------------------------------------------------------------------------------------
	//functions

	void init() {
		syseng::registerTablePointer(&g_events_table);
		syseng::registerTablePointer(&g_continuous_events_table);
		syseng::registerTablePointer(&g_handler_table);
		syseng::registerTablePointer(&g_subscribe_table);
	}

	void callAllEvents(VeFixedSizeTable<VeEventTableEntry>& events_table) {
		std::vector<VeHandlePair, custom_alloc<VeHandlePair>> result(getHeap());
		VeEventTableEntry eventData;
		VeEventSubscribeTableEntry subscribeData;
		VeEventHandlerTableEntry handlerData;

		events_table.leftJoin<std::multimap<VeHandle, VeIndex>, VeHandle, VeIndex>(0, g_subscribe_table, 0, result);
		for( auto [eventhandle, subscribehandle] : result ) {
			events_table.getEntry(eventhandle, eventData);
			g_subscribe_table.getEntry(subscribehandle, subscribeData);
			g_handler_table.getEntry(subscribeData.m_handlerH, handlerData);
			JADD( handlerData.m_handler(eventData) );
		}
	}

	void update() {
		return;
		callAllEvents(g_events_table);
		callAllEvents(g_continuous_events_table);
	}

	void cleanUp() {
	}

	void close() {
	}

	void addEvent(VeEventTableEntry event) {
		//std::cout << "add event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		g_events_table.insert( event );
	}

	void addContinuousEvent(VeEventTableEntry event) {
		//std::cout << "add cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		g_continuous_events_table.insert(event);
	}

	void removeContinuousEvent(VeEventTableEntry event) {
		//std::cout << "remove cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		VeHandle eventH = g_continuous_events_table.find(VeHandleTriple{ (VeIndex)event.m_type, event.m_key_button, event.m_action }, 1);
		assert(eventH != VE_NULL_HANDLE);
		g_continuous_events_table.erase(eventH);
	}

	void addHandler(std::function<void(VeEventTableEntry)> handler, VeHandle *pHandle) {
		g_handler_table.insert({handler}, pHandle);
	}

	void removeHandler(VeHandle handlerH) {
		std::vector<VeHandle, custom_alloc<VeHandle>> result(getHeap());
		g_subscribe_table.getHandlesEqual(1, handlerH, result);
		for (auto handle : result) {
			g_subscribe_table.erase(handle);
		}
		g_handler_table.erase(handlerH);
	}

	void subscribeEvent(VeHandle senderH, VeHandle receiverH ) {
		g_subscribe_table.insert({senderH, receiverH});
	}

	void unsubscribeEvent(VeHandle senderH, VeHandle receiverH) {
		VeHandle subH = g_subscribe_table.find(VeHandlePair{ senderH, receiverH }, 2);
		g_subscribe_table.erase(subH);
	}

}

