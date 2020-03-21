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
		VE_EVENT_TYPE_LOOP_TICK,			//once every game loop
		VE_EVENT_TYPE_EPOCH_TICK,			//per 1/60s epoch
		VE_EVENT_TYPE_FRAME_TICK,			//per rendered frame
		VE_EVENT_TYPE_ENTITY_CREATED,		//per entity
		VE_EVENT_TYPE_ENTITY_COLLIDED,
		VE_EVENT_TYPE_ENTITY_DESTROYED,
		VE_EVENT_TYPE_KEYBOARD,				//GLFW
		VE_EVENT_TYPE_MOUSEMOVE,
		VE_EVENT_TYPE_MOUSEBUTTON,
		VE_EVENT_TYPE_MOUSESCROLL,
		VE_EVENT_TYPE_LAST,
	};

	struct VeEventTableEntry {
		VeEventType	m_type = VeEventType::VE_EVENT_TYPE_NULL;
		//std::chrono::time_point<std::chrono::high_resolution_clock> m_eventTime = std::chrono::high_resolution_clock::now();

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
	void update();
	void cleanUp();
	void close();

	void addEvent(VeEventTableEntry event);
	void addHandler(std::function<void(VeEventTableEntry)> handler, VeHandle* pHandle);
	void addContinuousEvent(VeEventTableEntry event);
	void removeContinuousEvent(VeEventTableEntry event);
	void removeHandler(VeHandle handlerH);
	void subscribeEvent(VeEventType type, VeHandle handlerH);
	void unsubscribeEvent(VeHandle typeH, VeHandle handlerH);


#endif



}

