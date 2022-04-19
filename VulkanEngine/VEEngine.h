/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEENGINE_H
#define VEENGINE_H

#ifndef getEnginePointer
#define getEnginePointer() g_pVEEngineSingleton
#endif

namespace ve
{
	class VEWindow;

	class VERenderer;

	class VEEventListener;

	class VESceneManager;

	class VEEngine;

	extern VEEngine *g_pVEEngineSingleton; ///<Pointer to the only class instance

	/**
		*
		* \brief The engine core class.
		*
		* VEEngine is the core of the whole engine. It is responsible for creating instances of the other
		* management classes VEWindow, VERenderer, and VESceneManager, and running the render loop.
		* In the render loop, it also handles events and sends them to the registered event listeners.
		*
		*/

	class VEEngine
	{
		friend VEWindow;
		friend VERenderer;
		friend VEEventListener;
		friend VESceneManager;

	protected:
		VkInstance m_instance = VK_NULL_HANDLE; ///<Vulkan app instance
		VEWindow *m_pWindow = nullptr; ///<Pointer to the only Window instance

		veRendererType m_rendererType;
		VERenderer *m_pRenderer = nullptr; ///<Pointer to the only renderer instance

		VESceneManager *m_pSceneManager = nullptr; ///<Pointer to the only scene manager instance
		VkDebugReportCallbackEXT callback; ///<Debug callback handle

		std::vector<veEvent> m_eventlist; ///<List of events that should be handled in the next loop
		std::map<veEvent::veEventType, std::vector<VEEventListener *> *>
			m_eventListeners; ///<Maps event types to lists of evennt listeners

		double m_dt = 0.0; ///<Delta time since the last loop
		double m_time = 0.0; ///<Absolute game time since start of the render loop
		uint32_t m_loopCount = 0; ///<Counts up the render loop
		ThreadPool *m_threadPool; ///<thread pool for parallel processing

		//time statistics
		float m_AvgUpdateTime = 0.0f; ///<Average time for OBO updates (s)
		float m_AvgFrameTime = 0.0f; ///<Average time per frame (s)
		float m_MaxFrameTime = 0.0f; ///<Average time per frame (s)
		float m_AcceptableLevel1 = 1.0f; ///<percentage of frames with time < 33 ms (30 fps)
		float m_AcceptableLevel2 = 1.0f; ///<percentage of frames with time < 16 ms (60 fps)
		float m_AvgDrawTime = 0.0f; ///<Average time for baking cmd buffers and calling commit (s)
		float m_AvgStartedTime = 0.0f; ///<Average time for processing frame started event
		float m_AvgEventTime = 0.0f; ///<Average time for processing windows events
		float m_AvgEndedTime = 0.0f; ///<Average time for processing frame ended event
		float m_AvgPresentTime = 0.0f; ///<Average time for presenting the frame
		float m_AvgPrepOvlTime = 0.0f; ///<Average prepare overlay time
		float m_AvgDrawOvlTime = 0.0f; ///<Average draw overlay time

		bool m_framebufferResized = false; ///<Flag indicating whether the window size has changed.
		bool m_end_running = false; ///<Flag indicating that the engine should leave the render loop
		bool m_debug = true; ///<Flag indicating whether debugging is enabled or not

		virtual std::vector<const char *>
			getRequiredInstanceExtensions(); //Return a list of required Vulkan instance extensions
		virtual std::vector<const char *>
			getValidationLayers(); //Returns a list of required Vulkan validation layers
		void callListeners(double dt, veEvent event); //Call all event listeners and give them certain event
		void callListeners(double dt, veEvent event,
			std::vector<VEEventListener *> *list); //Call all event listeners and give them certain event
		void callListeners2(double dt, veEvent event, std::vector<VEEventListener *> *list, uint32_t startIdx, uint32_t endIdx);

		void processEvents(double dt); //Start handling all events
		void windowSizeChanged(); //Callback for window if window size has changed

		//startup routines, can be overloaded to create different managers
		virtual void createWindow(); //Create the only window
		virtual void createRenderer(); //Create the only renderer
		virtual void createSceneManager(); //Create the only scene manager
		virtual void registerEventListeners(); //Register all default event listeners, can be overloaded
		virtual void closeEngine(); //Close down the engine

	public:
		irrklang::ISoundEngine *m_irrklangEngine = irrklang::createIrrKlangDevice();

		VEEngine(veRendererType type, bool debug = false); //Only create ONE instance of the engine!
		~VEEngine() {};

		//-----------------------------------------------------------------------------------------------
		//managing the engine

		virtual void initEngine(); //Create all engine components
		virtual void run(); //Enter the render loop
		virtual void end(); //end the render loop
		virtual void loadLevel(uint32_t numLevel = 1); //load standard level with standard camera and lights

		//-----------------------------------------------------------------------------------------------
		//managing events and listeners

		void registerEventListener(VEEventListener *lis); //Register a new event listener.
		void registerEventListener(VEEventListener *lis, std::vector<veEvent::veEventType> eventTypes); //Register a new event listener for these events only
		VEEventListener *getEventListener(std::string name); //get pointer to an event listener
		void removeEventListener(std::string name); //Remove an event listener - it is NOT deleted automatically!
		void removeEventListener(std::string name, std::vector<VEEventListener *> *list); //Remove an event listener - it is NOT deleted automatically!
		void deleteEventListener(std::string name); //Delete an event listener
		void clearEventListenerList();

		void addEvent( veEvent event); //Add an event to the event list - will be handled in the next loop
		void deleteEvent(veEvent event); //Delete an event from the event list

		//-----------------------------------------------------------------------------------------------
		//get information and pointers

		VkInstance getInstance(); //Return the Vulkan instance
		VEWindow *getWindow(); //Return a pointer to the window instance
		VESceneManager *getSceneManager(); //Return a  pointer to the scene manager instance
		VERenderer *getRenderer(); //Return a pointer to the renderer instance
		veRendererType getRendererType();

		uint32_t getLoopCount(); //Return the number of the current render loop

		//-----------------------------------------------------------------------------------------------
		//thread pool

		///\returns the max number of threads that anybody may start during the render loop
		uint32_t getMaxThreads()
		{
			return (uint32_t)m_threadPool->threadCount();
		};

		///\returns a pointer to the threadpool
		ThreadPool *getThreadPool()
		{
			return m_threadPool;
		};

		//-----------------------------------------------------------------------------------------------
		//time statistics

		///\returns the average frame time (s)
		float getAvgFrameTime()
		{
			return m_AvgFrameTime;
		};

		///\returns the maximal frame time (s)
		float getMaxFrameTime()
		{
			return m_MaxFrameTime;
		};

		///\returns the percentage of frames with frame time < 33 ms (s)
		float getAcceptableLevel1()
		{
			return m_AcceptableLevel1;
		};

		///\returns the percentage of frames with frame time < 16 ms (s)
		float getAcceptableLevel2()
		{
			return m_AcceptableLevel2;
		};

		///\returns the average update time (s)
		float getAvgUpdateTime()
		{
			return m_AvgUpdateTime;
		};

		///\returns the average started time (s)
		float getAvgStartedTime()
		{
			return m_AvgStartedTime;
		};

		///\returns the average event time (s)
		float getAvgEventTime()
		{
			return m_AvgEventTime;
		};

		///\returns the average event time (s)
		float getAvgDrawTime()
		{
			return m_AvgDrawTime;
		};

		///\returns the average ended time (s)
		float getAvgEndedTime()
		{
			return m_AvgEndedTime;
		};

		///\returns the average present time (s)
		float getAvgPresentTime()
		{
			return m_AvgPresentTime;
		};

		///\returns the average prep overlay time (s)
		float getAvgPrepOvlTime()
		{
			return m_AvgPrepOvlTime;
		};

		///\returns the average draw overlay time (s)
		float getAvgDrawOvlTime()
		{
			return m_AvgDrawOvlTime;
		};

		bool isRayTracing()
		{
			return m_rendererType == VE_RENDERER_TYPE_RAYTRACING_NV ||
				m_rendererType == VE_RENDERER_TYPE_RAYTRACING_KHR;
		}
	};

} // namespace ve

#endif
