#pragma once


namespace vve::syseve {

	enum VeEventType {
		VE_EVENT_TYPE_NULL=0,
		VE_EVENT_TYPE_FRAME_TICK,
		VE_EVENT_TYPE_KEYBOARD,
		VE_EVENT_TYPE_MOUSEMOVE,
		VE_EVENT_TYPE_MOUSEBUTTON,
		VE_EVENT_TYPE_MOUSESCROLL,
		VE_EVENT_TYPE_LAST,
	};

	struct VeEventTypeTableEntry {
		VeIndex m_type;
	};

	struct VeEventTableEntry {
		VeHandle	m_typeH;
		VeIndex		m_key_button;
		VeIndex		m_scancode;
		VeIndex		m_action;
		VeIndex		m_mods;
		double		m_x;
		double		m_y;
	};

	struct VeEventSubscribeTableEntry {
		VeHandle m_typeH;
		VeHandle m_handlerH;
	};

	struct VeEventHandlerTableEntry {
		std::function<void(VeEventTableEntry)> m_handler;
	};


#ifndef VE_PUBLIC_INTERFACE

	void init();
	void tick();
	void close();

	void addEvent(VeEventType type, VeEventTableEntry event);
	void addHandler(std::function<void(VeEventTableEntry)> handler);
	void removeHandler(VeHandle handlerH);
	void subscribeEvent(VeEventType type, VeHandle handlerH);
	void unsubscribeEvent(VeHandle typeH, VeHandle handlerH);


#endif



}

