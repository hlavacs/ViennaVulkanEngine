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
		VeHandle	m_senderH = VE_NULL_HANDLE;
		VeHandle	m_receiverH = VE_NULL_HANDLE;
		VeHandle	m_dataH = VE_NULL_HANDLE;

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

	VeHandle addMessage(VeMessageTableEntry message, VeHandle *pHandle = nullptr);
	VeHandle addHandler(std::function<void(VeMessageTableEntry)> handler, VeHandle* pHandle = nullptr);
	VeHandle addContinuousMessage(VeMessageTableEntry message, VeHandle* pHandle = nullptr);
	void removeContinuousMessage(VeMessageTableEntry message);
	void removeHandler(VeHandle handlerH);
	void subscribeMessage(VeHandle senderH, VeHandle receiverH, VeHandle handlerH, VeMessageType type = VeMessageType::VE_MESSAGE_TYPE_NULL, VeIndex thread_id = VE_NULL_INDEX);
	void unsubscribeMessage(VeHandle senderH, VeHandle handlerH);


#endif



}

