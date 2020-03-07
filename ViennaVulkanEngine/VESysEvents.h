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

	enum VeEventType {
		VE_EVENT_TYPE_NULL=0,
		VE_EVENT_TYPE_FRAME_TICK,			//per frame
		VE_EVENT_TYPE_ENTITY_CREATED,		//per entity
		VE_EVENT_TYPE_ENTITY_DESTROYED,
		VE_EVENT_TYPE_ENTITY_COLLIDED,
		VE_EVENT_TYPE_KEYBOARD,				//GLFW
		VE_EVENT_TYPE_MOUSEMOVE,
		VE_EVENT_TYPE_MOUSEBUTTON,
		VE_EVENT_TYPE_MOUSESCROLL,
		VE_EVENT_TYPE_LAST,
	};

	struct VeEventTableEntry {
		VeHandle	m_typeH = VE_NULL_HANDLE;

		//--------------------------------------------
		VeHandle	m_entityH = VE_NULL_HANDLE;
		VeHandle	m_entityOtherH = VE_NULL_HANDLE;

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
	void tick();
	void close();

	void addEvent(VeEventType type, VeEventTableEntry event);
	void addHandler(std::function<void(VeEventTableEntry)> handler);
	void addContinuousEvent(VeEventType type, VeEventTableEntry event);
	void removeContinuousEvent(VeEventType type, VeEventTableEntry event);
	void removeHandler(VeHandle handlerH);
	void subscribeEvent(VeEventType type, VeHandle handlerH);
	void unsubscribeEvent(VeHandle typeH, VeHandle handlerH);


#endif



}

