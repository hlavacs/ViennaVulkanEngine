/**
*
* \file
* \brief
*
* Details
*
*/


#include "VEDefines.h"
#include "VESysMessages.h"
#include "VESysEngine.h"


namespace vve::sysmes {


	//--------------------------------------------------------------------------------------------------
	//messages 

	std::vector<VeMap*> maps1 = {
		new VeHashedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(VeMessageTableEntry, m_senderH), (VeIndex)sizeof(VeMessageTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >(
			VeIndexPair((VeIndex)offsetof(VeMessageTableEntry, m_senderH), (VeIndex)offsetof(VeMessageTableEntry, m_receiverH) ),
			VeIndexPair((VeIndex)sizeof(VeMessageTableEntry::m_senderH), (VeIndex)sizeof(VeMessageTableEntry::m_receiverH) ) )
	};
	VeFixedSizeTableMT<VeMessageTableEntry> g_messages_table( "Messages Table", maps1, true, true, 0, 0);
	VeFixedSizeTableMT<VeMessageTableEntry> g_messages_table2(g_messages_table);


	std::vector<VeMap*> maps2 = {
		new VeHashedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(VeMessageTableEntry, m_senderH), (VeIndex)sizeof(VeMessageTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >(
			VeIndexPair((VeIndex)offsetof(VeMessageTableEntry, m_senderH), (VeIndex)offsetof(VeMessageTableEntry, m_receiverH)),
			VeIndexPair((VeIndex)sizeof(VeMessageTableEntry::m_senderH), (VeIndex)sizeof(VeMessageTableEntry::m_receiverH)))
	};
	VeFixedSizeTableMT<VeMessageTableEntry> g_continuous_messages_table("Continuous Messages Table", maps2, true, false, 0, 0);
	VeFixedSizeTableMT<VeMessageTableEntry> g_continuous_messages_table2(g_continuous_messages_table);


	//--------------------------------------------------------------------------------------------------
	//message handler

	struct VeMessageHandlerTableEntry {
		std::function<void(VeMessageTableEntry)> m_handler;
	};
	VeFixedSizeTableMT<VeMessageHandlerTableEntry> g_handler_table("Message Handler Table", false, false, 0, 0);
	VeFixedSizeTableMT<VeMessageHandlerTableEntry> g_handler_table2(g_handler_table);


	//--------------------------------------------------------------------------------------------------
	//subscribers

	struct VeMessageSubscribeTableEntry {
		VeHandle	m_senderH;		//handle of the sending entity
		VeHandle	m_receiverH;	//handle of the receiver entity
		VeHandle	m_handlerH;		//handler of this message
		VeMessageType	m_type;
		VeIndex		m_thread_id;	//run handler function in this thread or NULL
	};
	std::vector<VeMap*> maps4 = {
		new VeHashedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(VeMessageSubscribeTableEntry, m_senderH), (VeIndex)sizeof(VeMessageSubscribeTableEntry::m_senderH)),
		new VeHashedMultimap< VeHandle, VeIndex >((VeIndex)offsetof(VeMessageSubscribeTableEntry, m_handlerH), (VeIndex)sizeof(VeMessageSubscribeTableEntry::m_handlerH)),
		new VeHashedMultimap< VeHandlePair, VeIndexPair >
			(VeIndexPair{(VeIndex)offsetof(VeMessageSubscribeTableEntry, m_senderH), (VeIndex)offsetof(VeMessageSubscribeTableEntry, m_handlerH)},
			 VeIndexPair{(VeIndex)sizeof(VeMessageSubscribeTableEntry::m_senderH),   (VeIndex)sizeof(VeMessageSubscribeTableEntry::m_handlerH)})
	};
	VeFixedSizeTableMT<VeMessageSubscribeTableEntry> g_subscribe_table("Message Subscribe Table", maps4, true, false, 0, 0);
	VeFixedSizeTableMT<VeMessageSubscribeTableEntry> g_subscribe_table2(g_subscribe_table);


	//--------------------------------------------------------------------------------------------------
	//functions

	void init() {
		syseng::registerEntity(VE_SYSTEM_NAME);
		VE_SYSTEM_HANDLE = syseng::getEntityHandle(VE_SYSTEM_NAME);

		syseng::registerTablePointer(&g_messages_table);
		syseng::registerTablePointer(&g_continuous_messages_table);
		syseng::registerTablePointer(&g_handler_table);
		syseng::registerTablePointer(&g_subscribe_table);
	}

	void callAllMessages(	VeFixedSizeTableMT<VeMessageTableEntry>& messages_table ) {
		std::vector<VeHandlePair, custom_alloc<VeHandlePair>> result(getHeap());
		VeMessageTableEntry messageData;
		VeMessageSubscribeTableEntry subscribeData;
		VeMessageHandlerTableEntry handlerData;

		messages_table.leftJoin(0, &g_subscribe_table, 0, result);
		for( auto [messagehandle, subscribehandle] : result ) {
			messages_table.getEntry(messagehandle, messageData);
			g_subscribe_table.getEntry(subscribehandle, subscribeData);

			bool recvOK = (subscribeData.m_receiverH == VE_NULL_HANDLE || subscribeData.m_receiverH == messageData.m_receiverH);
			bool typeOK = (subscribeData.m_type == VeMessageType::VE_MESSAGE_TYPE_NULL || subscribeData.m_type == messageData.m_type);
			if ( recvOK && typeOK) {
				g_handler_table.getEntry(subscribeData.m_handlerH, handlerData);
				JADD(handlerData.m_handler(messageData));
			}
		}
	}

	void update() {
		callAllMessages( g_messages_table );
		callAllMessages( g_continuous_messages_table );
	}

	void close() {
	}

	VeHandle addMessage(VeMessageTableEntry message, VeHandle* pHandle ) {
		//std::cout << "add message type " << message.m_type << " action " << message.m_action << " key/button " << message.m_key_button << std::endl;
		return g_messages_table.insert( message, pHandle );
	}

	VeHandle addContinuousMessage(VeMessageTableEntry message, VeHandle * pHandle ) {
		//std::cout << "add cont message type " << message.m_type << " action " << message.m_action << " key/button " << message.m_key_button << std::endl;
		return g_continuous_messages_table.insert(message);
	}

	void removeContinuousMessage(VeMessageTableEntry message) {
		//std::cout << "remove cont message type " << message.m_type << " action " << message.m_action << " key/button " << message.m_key_button << std::endl;
		VeHandle messageH = g_continuous_messages_table.find(VeHandlePair{  }, 1);
		assert(messageH != VE_NULL_HANDLE);
		g_continuous_messages_table.erase(messageH);
	}

	VeHandle addHandler(std::function<void(VeMessageTableEntry)> handler, VeHandle *pHandle) {
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

	void subscribeMessage(VeHandle senderH, VeHandle receiverH, VeHandle handlerH, 
						VeMessageType type, VeIndex thread_id ) {

		g_subscribe_table.insert({senderH, receiverH, handlerH, type, thread_id });
	}

	void unsubscribeMessage(VeHandle senderH, VeHandle receiverH) {
		VeHandle subH = g_subscribe_table.find(VeHandlePair{ senderH, receiverH }, 2);
		g_subscribe_table.erase(subH);
	}

}

