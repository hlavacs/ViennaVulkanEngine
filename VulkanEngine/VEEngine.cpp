/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define VMA_IMPLEMENTATION

#include "VEInclude.h"


using namespace vh;

namespace ve {
	class VEWindow;

	VEEngine * g_pVEEngineSingleton = nullptr;	///<Singleton pointer to the only VEEngine instance

	/**
	* \brief Constructor of my VEEngine
	* \param[in] debug Switch debuggin on or off
	*/ 
	VEEngine::VEEngine(bool debug) : m_debug(debug) {
		g_pVEEngineSingleton = this; 
	}

	//-------------------------------------------------------------------------------------------------------
	//init the engine

	/**
	* \brief Initialize the engine
	*
	* Creates a VEREnderer, VESceneManager, VEWindow. Then initializes Vulkan and gets the Vulkan instance.
	* After this, initializes VERenderer, VESceneManager and VEWindow. Then it registers default event listeners.
	*/
	void VEEngine::initEngine() {
		createRenderer();					//create a renderer
		createSceneManager();				//create a scene manager
		createWindow();						//create a window
		m_pWindow->initWindow(800, 600);	//inittialize the window

		m_threadPool = new ThreadPool(20); //worker threads
		m_threadPool->init();

		std::vector<const char*> instanceExtensions = getRequiredInstanceExtensions();
		std::vector<const char*> validationLayers = getValidationLayers();
		vhDevCreateInstance(instanceExtensions, validationLayers, &m_instance);
		
		if( m_debug )
			vh::vhSetupDebugCallback( m_instance, &callback );				//create a debug callback for printing debug information
		
		m_pWindow->createSurface(m_instance, &m_pRenderer->m_surface);	//create a Vulkan surface
		m_pRenderer->initRenderer();		//initialize the renderer
		m_pSceneManager->initSceneManager();	//initialize the scene manager

		registerEventListeners();
	}


	/**
	* \brief Create the only VEWindow instance and store a pointer to it.
	*
	* VEWindow is a base class only and should not be created itself. Instead, a derived class should be 
	* instanciated, being tailored to one specific windowing system.
	*
	*/
	void VEEngine::createWindow() {
		m_pWindow = new VEWindowGLFW();
	}

	/**
	* \brief Create the only VERenderer instance and store a pointer to it.
	*
	* VERenderer is a base class only and should not be created itself. Instead, a derived class should be
	* instanciated, being tailored to one specific windowing system.
	*
	*/
	void VEEngine::createRenderer() {
		m_pRenderer = new VERendererForward();
	}

	/**
	* \brief Create the only VESceneManager instance and store a pointer to it.
	*/
	void VEEngine::createSceneManager() {
		m_pSceneManager = new VESceneManager();
	}


	/**
	*
	* Register default event listener for steering the standard camera
	* overload if you do not want this event listener to be registered.
	*
	*/
	void VEEngine::registerEventListeners() {
		registerEventListener( new VEEventListenerGLFW("StandardEventListener")); //register a standard listener
	}


	/**
	* \brief Close the engine, delete all resources.
	*/
	void VEEngine::closeEngine() {

		m_threadPool->shutdown();
		delete m_threadPool;

		for (auto el : m_eventListener) delete el;
		m_eventListener.clear();
		m_eventlist.clear();

		m_pSceneManager->closeSceneManager();
		m_pRenderer->closeRenderer();
		m_pWindow->closeWindow();

		if( m_debug )
			vhDebugDestroyReportCallbackEXT(m_instance, callback, nullptr);

		vkDestroySurfaceKHR(m_instance, m_pRenderer->m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	//-------------------------------------------------------------------------------------------------------
	//requirements

	/**
	*
	* \returns a list of Vulkan API validation layers that should be activated.
	*
	*/
	std::vector<const char*> VEEngine::getValidationLayers() {
		std::vector<const char*> validationLayers = {};
		if (m_debug) {
			validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
		}
		return validationLayers;
	}

	/**
	* \returns a list of Vulkan API instance extensions that are required.
	*/	
	std::vector<const char*> VEEngine::getRequiredInstanceExtensions() {
		std::vector<const char*> extensions = m_pWindow->getRequiredInstanceExtensions();
		if (m_debug) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			extensions.push_back("VK_EXT_debug_report");
		}
		return extensions;
	};


	//-------------------------------------------------------------------------------------------------------
	//event handling

	/**
	*
	* \brief Register an event listener.
	*
	* The event listener will be sent events to in the render loop.
	*
	* \param[in] pListener Pointer to the event listener to be registered.
	*
	*/
	void VEEngine::registerEventListener(VEEventListener *pListener) {
		m_eventListener.push_back( pListener );
	}; 

	/**
	*
	* \brief Remove an event listener.
	*
	* The event listener will be removed fróm the list but not destroyed.
	*
	* \param[in] name The name of the event listener to be removed.
	*
	*/
	void VEEngine::removeEventListener(std::string name) {
		for (uint32_t i = 0; i < m_eventListener.size(); i++) {
			if (m_eventListener[i]->getName() == name) {
				m_eventListener[i] = m_eventListener[m_eventListener.size() - 1];		//write over last listener
				m_eventListener.pop_back();
				return;
			}
		}
	};

	/**
	*
	* \brief Delete an event listener.
	*
	* The event listener will be removed from the list and destroyed.
	*
	*/
	void VEEngine::deleteEventListener(std::string name)  {
		for (uint32_t i = 0; i < m_eventListener.size(); i++) {
			if (m_eventListener[i]->getName() == name) {
				delete m_eventListener[i];											//delete the pointer
				m_eventListener[i] = m_eventListener[m_eventListener.size()-1];		//write over last listener
				m_eventListener.pop_back();
				return;
			}
		}
	};

	/**
	*
	* \brief Call all event listeners.
	*
	* This function calls all event listeners and gives them a specific event. The listeners can react to the 
	* event or not. If one listener retursn true, then event is "consumed" by this listener, and the 
	* function stops. Otherwise it continues and goes on calling listeners.
	*
	* \param[in] dt Delta time that passed by since the last loop.
	* \param[in] event The event that should be passed to all listeners.
	*
	*/
	void VEEngine::callListeners(double dt, veEvent event) {
		event.dt = dt;

		if ( false) {		//m_eventListener.size() > 0) { ToDO: fix VEEntity::updateChildren 
			uint32_t numThreads = 10;
			uint32_t numListenerPerThread = (uint32_t)m_eventListener.size() / numThreads;
			uint32_t numListenerLastThread = (uint32_t)m_eventListener.size() - numListenerPerThread * (numThreads - 1);

			uint32_t startIdx;
			uint32_t endIdx;
			for (uint32_t k = 0; k < numThreads; k++) {
				startIdx = k*numListenerPerThread;
				endIdx = k < numThreads - 1 ? numListenerPerThread : numListenerLastThread;
				m_threadPool->submit([=]() { this->callListeners(dt, event, startIdx, endIdx); });
			}
		}
		else {
			if(m_eventListener.size()>0) callListeners( dt, event, 0, (uint32_t) m_eventListener.size() - 1);
		}

	}

	/**
	*
	* \brief Call a subset of the listeners, which can be done in parallel
	*
	* \param[in] dt Delta time that passed by since the last loop.
	* \param[in] event The event that should be processed.
	* \param[in] startIdx Index of the first listener to call
	* \param[in] endIdx Index of the last listener to call
	*
	*/
	void VEEngine::callListeners(double dt, veEvent event, uint32_t startIdx, uint32_t endIdx) {
		for( uint32_t i=startIdx; i<=endIdx; i++ ) {
			if (m_eventListener[i]->onEvent(event)) return;		//if return true then the listener has consumed the event
		}
	}



	/**
	*
	* \brief Add an event to the event list.
	*
	* This adds a new event that should be processed in the next iteration.
	*
	* \param[in] event The event that should be processed.
	*
	*/
	void VEEngine::addEvent(veEvent event) {
		m_eventlist.push_back(event);
	}

	/**
	*
	* \brief Delete an event from the event list.
	*
	* This removes an event from the event list. Mainly this is used to remove continuous events like a key
	* is pressed or a mouse button is clicked.
	*
	* \param[in] event The event that should be removed.
	*
	*/
	void VEEngine::deleteEvent(veEvent event) {
		for (uint32_t i = 0; i < m_eventlist.size(); i++) {
			if (event.type == m_eventlist[i].type &&
				event.subsystem == m_eventlist[i].subsystem &&
				event.idata1 == m_eventlist[i].idata1 &&
				event.idata2 == m_eventlist[i].idata2 &&
				event.idata3 == m_eventlist[i].idata3 &&
				event.idata4 == m_eventlist[i].idata4
				) {
				uint32_t last = (uint32_t)m_eventlist.size() - 1;
				m_eventlist[i] = m_eventlist[last];
				m_eventlist.pop_back();
			}
		}
	}


	/**
	*
	* \brief Go through the event list, and call all listeners for it.
	*
	* Go through all events in the event list and pass them to the event listeners. Continuous events are kept,
	* all other events are deleted from the list at the end.
	*
	* \param[in] dt The delta time that has passed since the last loop.
	*
	*/
	void VEEngine::processEvents( double dt) {

		std::vector<veEvent> keepEvents;							//Keep these events in the list
		for (auto event : m_eventlist) {
			if (event.lifeTime == VE_EVENT_LIFETIME_CONTINUOUS) {
				keepEvents.push_back(event);
			}
		}

		for (auto event : m_eventlist) {
			if ( event.notBeforeTime <= m_loopCount) {
				callListeners(dt, event);
			}
		}
		m_eventlist.clear();

		for (auto event : keepEvents) {
			m_eventlist.push_back(event);
		}
	}

		

	/**
	*
	* \brief Inform the engine that the window size has been changed.
	*
	* If the window size has been changed, then the renderer has to change many resources like the swap chain size,
	* framebuffers etc. 
	*
	*/
	void VEEngine::windowSizeChanged() {
		m_framebufferResized = true;
	};


	//-------------------------------------------------------------------------------------------------------
	//getter functions

	/**
	* \returns the Vulkan instance.
	*/
	VkInstance VEEngine::getInstance() {
		return m_instance; 
	}
	
	/**
	* \returns the VEWindow instance.
	*/
	VEWindow * VEEngine::getWindow() {
		return m_pWindow; 
	}
	
	/**
	* \returns the VESceneManager instance.
	*/
	VESceneManager * VEEngine::getSceneManager() {
		return m_pSceneManager; 
	};

	/**
	* \returns the VERenderer instance.
	*/
	VERenderer  * VEEngine::getRenderer() {
		return m_pRenderer; 
	}

	/**
	* \returns the loop counter.
	*/
	uint32_t VEEngine::getLoopCount() {
		return m_loopCount; 
	};


	//-------------------------------------------------------------------------------------------------------
	//render loop

	/**
	*
	* \brief The main render loop.
	*
	* This is the main render loop. It performs time measurements, runs the event processing, checks whether the window
	* size has changed (if so, informs the VEREnderer), and asks the VERenderer to draw and present one frame.
	*
	*/
	void VEEngine::run() {
		std::chrono::high_resolution_clock::time_point t_start = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point t_prev = t_start;

		while ( !m_end_running) {
			std::chrono::high_resolution_clock::time_point t_now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t_now - t_prev);
			m_dt = time_span.count();	//time since last frame

			time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t_now - t_start);
			m_time = time_span.count();		//time since program start
			t_prev = t_now;

			veEvent event(VE_EVENT_FRAME_STARTED);	//notify all listeners that a new frame starts
			callListeners(m_dt, event);

			m_pWindow->pollEvents();			//poll window events which should be injected into the event queue

			if (m_framebufferResized) {			//if window size changed, recreate the current swapchain
				m_pWindow->waitForWindowSizeChange();
				m_pRenderer->recreateSwapchain();
				m_framebufferResized = false;
			}
			
			processEvents(m_dt);				//process all current events, including pressed keys

			m_pRenderer->drawFrame();			//draw the next frame

			m_pRenderer->prepareOverlay();

			event.type = VE_EVENT_FRAME_ENDED;	//notify all listeners that the frame ended
			callListeners(m_dt, event);

			m_pRenderer->drawOverlay();

			m_pRenderer->presentFrame();		//present the next frame

			m_loopCount++;
		}
		closeEngine();

	}

	/**
	*
	* \brief End the main render loop.
	*
	* This sets a flag to tell the main render loop to stop.
	*
	*/
	void VEEngine::end() {
		m_end_running = true; 
	}




}




