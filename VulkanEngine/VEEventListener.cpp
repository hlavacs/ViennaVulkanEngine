/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	///Constructor
	VEEventListener::VEEventListener(std::string name)
		: VENamedClass(name) {};

	///Destructor
	VEEventListener::~VEEventListener() {};

	/**
		*
		* \brief Use the main event type to call specific handlers
		*
		* If an event handler consumes an event, it must return true. This means that this event is not
		* sent to the remaining handlers.
		*
		* \param[in] event The event that should be examined and processed
		* \returns true if the event is consumed, else false
		*/
	bool VEEventListener::onEvent(veEvent event)
	{
		switch (event.type)
		{
		case veEvent::VE_EVENT_FRAME_STARTED:
			onFrameStarted(event);
			return false;
			break;
		case veEvent::VE_EVENT_FRAME_ENDED:
			onFrameEnded(event);
			return false;
			break;
		case veEvent::VE_EVENT_DRAW_OVERLAY:
			onDrawOverlay(event);
			return false;
			break;
		case veEvent::VE_EVENT_KEYBOARD:
			return onKeyboard(event);
			break;
		case veEvent::VE_EVENT_MOUSEMOVE:
			return onMouseMove(event);
			break;
		case veEvent::VE_EVENT_MOUSEBUTTON:
			return onMouseButton(event);
			break;
		case veEvent::VE_EVENT_MOUSESCROLL:
			return onMouseScroll(event);
			break;
		case veEvent::VE_EVENT_DELETE_NODE:
			return onSceneNodeDeleted(event);
			break;

		default:
			break;
		}
		return false;
	}

} // namespace ve
