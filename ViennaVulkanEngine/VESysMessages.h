#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::sysmes {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM MESSAGES";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	enum class VeMessageType {
		VE_MESSAGE_TYPE_NULL=0,
		VE_MESSAGE_TYPE_UPDATE,				//per rendered frame
		VE_MESSAGE_TYPE_ENTITY_CREATED,		//per entity
		VE_MESSAGE_TYPE_ENTITY_COLLIDED,
		VE_MESSAGE_TYPE_ENTITY_DESTROYED,
		VE_MESSAGE_TYPE_KEYBOARD,				//GLFW
		VE_MESSAGE_TYPE_MOUSEMOVE,
		VE_MESSAGE_TYPE_MOUSEBUTTON,
		VE_MESSAGE_TYPE_MOUSESCROLL,
		VE_MESSAGE_TYPE_CLOSE,
		VE_MESSAGE_TYPE_LAST
	};

	struct VeMessageTableEntry {
		VeMessageType	m_type = VeMessageType::VE_MESSAGE_TYPE_NULL;
		VeHandle		m_senderID = VE_NULL_HANDLE;
		VeHandle		m_receiverID = VE_NULL_HANDLE;
		VeHandle		m_dataID = VE_NULL_HANDLE;

		//--------------------------------------------
		VeIndex		m_key_button = 0;
		VeIndex		m_scancode = 0;
		VeIndex		m_action = 0;
		VeIndex		m_mods = 0;
		double		m_x = 0.0;
		double		m_y = 0.0;
	};



#ifndef VE_PUBLIC_INTERFACE

	void init();
	void update();
	void close();

	VeHandle sendMessage(VeMessageTableEntry message);
	void receiveMessages( VeHandle receiverID, std::vector<VeHandle, custom_alloc<VeHandle>> &result);
	bool getMessage(VeHandle messageID, VeMessageTableEntry &entry);
	VeHandle addContinuousMessage(VeMessageTableEntry message);
	void removeContinuousMessage(VeMessageTableEntry message);
	VeHandle addHandler(std::function<void(VeHandle)> handler);
	void removeHandler(VeHandle handlerID);
	void subscribeMessage(VeHandle senderID, VeHandle receiverID, VeHandle handlerID, 
		VeMessageType type = VeMessageType::VE_MESSAGE_TYPE_NULL, vgjs::VgjsThreadIndex thread_id = vgjs::VGJS_NULL_THREAD_IDX);
	void unsubscribeMessage(VeHandle senderID, VeHandle handlerID, VeMessageType type = VeMessageType::VE_MESSAGE_TYPE_NULL);


#endif



}

