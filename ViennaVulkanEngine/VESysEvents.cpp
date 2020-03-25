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
		new VeHashedMultimap< VeHandle, VeIndex >(offsetof(VeEventTableEntry, m_senderH), sizeof(VeEventTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >(
			VeIndexPair((VeIndex)offsetof(VeEventTableEntry, m_senderH), (VeIndex)offsetof(VeEventTableEntry, m_receiverH) ),
			VeIndexPair((VeIndex)sizeof(VeEventTableEntry::m_senderH), (VeIndex)sizeof(VeEventTableEntry::m_receiverH) ) )
	};
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table( "Events Table", maps1, true, true, 0, 0);
	VeFixedSizeTableMT<VeEventTableEntry> g_events_table2(g_events_table);


	std::vector<VeMap*> maps2 = {
		new VeHashedMultimap< VeHandle, VeIndex >(offsetof(VeEventTableEntry, m_senderH), sizeof(VeEventTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >(
			VeIndexPair((VeIndex)offsetof(VeEventTableEntry, m_senderH), (VeIndex)offsetof(VeEventTableEntry, m_receiverH)),
			VeIndexPair((VeIndex)sizeof(VeEventTableEntry::m_senderH), (VeIndex)sizeof(VeEventTableEntry::m_receiverH)))
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
		VeHandle	m_senderH;		//handle of the sending entity
		VeHandle	m_receiverH;	//handle of the receiver entity
		VeHandle	m_handlerH;		//handler of this message
		VeEventType	m_type;
		VeIndex		m_thread_id;	//run handler function in this thread or NULL
	};
	std::vector<VeMap*> maps4 = {
		new VeHashedMultimap< VeHandle, VeIndex >(offsetof(VeEventSubscribeTableEntry, m_senderH), sizeof(VeEventSubscribeTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandle, VeIndex >(offsetof(VeEventSubscribeTableEntry, m_handlerH), sizeof(VeEventSubscribeTableEntry::m_handlerH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >
			(VeIndexPair{(VeIndex)offsetof(VeEventSubscribeTableEntry, m_senderH), (VeIndex)offsetof(VeEventSubscribeTableEntry, m_handlerH)},
			 VeIndexPair{(VeIndex)sizeof(VeEventSubscribeTableEntry::m_senderH),   (VeIndex)sizeof(VeEventSubscribeTableEntry::m_handlerH)})
	};
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table("Event Subscribe Table", maps4, true, false, 0, 0);
	VeFixedSizeTableMT<VeEventSubscribeTableEntry> g_subscribe_table2(g_subscribe_table);


	//--------------------------------------------------------------------------------------------------
	//functions

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

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

			if (subscribeData.m_type == VeEventType::VE_EVENT_TYPE_NULL || subscribeData.m_type == eventData.m_type) {
				g_handler_table.getEntry(subscribeData.m_handlerH, handlerData);
				JADD(handlerData.m_handler(eventData));
			}
		}
	}

	void update() {
		callAllEvents(g_events_table);
		callAllEvents(g_continuous_events_table);
	}

	void cleanUp() {
	}

	void close() {
	}

	VeHandle addEvent(VeEventTableEntry event, VeHandle* pHandle ) {
		//std::cout << "add event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		return g_events_table.insert( event, pHandle );
	}

	VeHandle addContinuousEvent(VeEventTableEntry event, VeHandle * pHandle ) {
		//std::cout << "add cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		return g_continuous_events_table.insert(event);
	}

	void removeContinuousEvent(VeEventTableEntry event) {
		//std::cout << "remove cont event type " << event.m_type << " action " << event.m_action << " key/button " << event.m_key_button << std::endl;
		VeHandle eventH = g_continuous_events_table.find(VeHandlePair{  }, 1);
		assert(eventH != VE_NULL_HANDLE);
		g_continuous_events_table.erase(eventH);
	}

	VeHandle addHandler(std::function<void(VeEventTableEntry)> handler, VeHandle *pHandle) {
		return g_handler_table.insert({handler}, pHandle);
	}

	void removeHandler(VeHandle handlerH) {
		std::vector<VeHandle, custom_alloc<VeHandle>> result(getHeap());
		g_subscribe_table.getHandlesEqual(1, handlerH, result);
		for (auto handle : result) {
			g_subscribe_table.erase(handle);
		}
		g_handler_table.erase(handlerH);
	}

	void subscribeEvent(VeHandle senderH, VeHandle receiverH, VeHandle handlerH, 
						VeEventType type, VeIndex thread_id ) {

		g_subscribe_table.insert({senderH, receiverH, handlerH, type, thread_id });
	}

	void unsubscribeEvent(VeHandle senderH, VeHandle receiverH) {
		VeHandle subH = g_subscribe_table.find(VeHandlePair{ senderH, receiverH }, 2);
		g_subscribe_table.erase(subH);
	}

}

