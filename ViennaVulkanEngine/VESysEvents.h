#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


namespace vve::syseve {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM MESSAGES";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	enum class VeEventType {
		VE_EVENT_TYPE_NULL=0,
		VE_EVENT_TYPE_UPDATE,				//per rendered frame
		VE_EVENT_TYPE_ENTITY_CREATED,		//per entity
		VE_EVENT_TYPE_ENTITY_COLLIDED,
		VE_EVENT_TYPE_ENTITY_DESTROYED,
		VE_EVENT_TYPE_KEYBOARD,				//GLFW
		VE_EVENT_TYPE_MOUSEMOVE,
		VE_EVENT_TYPE_MOUSEBUTTON,
		VE_EVENT_TYPE_MOUSESCROLL,
		VE_EVENT_TYPE_CLOSE,
		VE_EVENT_TYPE_LAST
	};

	struct VeEventTableEntry {
		VeEventType	m_type = VeEventType::VE_EVENT_TYPE_NULL;
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
	void cleanUp();
	void close();

	void addEvent(VeEventTableEntry event);
	void addHandler(std::function<void(VeEventTableEntry)> handler, VeHandle* pHandle);
	void addContinuousEvent(VeEventTableEntry event);
	void removeContinuousEvent(VeEventTableEntry event);
	void removeHandler(VeHandle handlerH);
	void subscribeEvent(VeHandle senderH, VeHandle receiverH, VeHandle handlerH, VeIndex thread_id);
	void unsubscribeEvent(VeHandle senderH, VeHandle handlerH);


#endif



}

