#pragma once


namespace vve::syseve {

	enum VeEventType {
		VE_EVENT_TYPE_NULL=0,
		VE_EVENT_TYPE_FRAME_TICK,
		VE_EVENT_TYPE_KEYBOARD,
		VE_EVENT_TYPE_MOUSEMOVE,
		VE_EVENT_TYPE_MOUSEBUTTON,
		VE_EVENT_TYPE_MOUSESCROLL
	};

	struct VeEventTableEntry {
		VeEventType m_type;
		VeIndex		m_key_button;
		VeIndex		m_scancode;
		VeIndex		m_action;
		VeIndex		m_mods;
		double		m_x;
		double		m_y;
	};

	struct VeEventRegisteredHandlerTableEntry {
		VeEventType m_type;
		std::function<void(VeEventTableEntry ev)> m_handler;
	};


#ifndef VE_PUBLIC_INTERFACE

	void init();
	void tick();
	void close();

	void addEvent(VeEventTableEntry event);
	void addHandler(VeEventType type, std::function<void(VeHandle handle)> m_handler);


#endif



}

