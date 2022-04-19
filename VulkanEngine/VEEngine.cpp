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

namespace ve
{
	class VEWindow;

	VEEngine *g_pVEEngineSingleton = nullptr; ///<Singleton pointer to the only VEEngine instance

	/**
		* \brief Constructor of my VEEngine
		* \param[in] debug Switch debuggin on or off
		*/
	VEEngine::VEEngine(veRendererType type, bool debug)
		: m_debug(debug),
		m_rendererType(type)
	{
		g_pVEEngineSingleton = this;
		if (vhLoadVulkanLibrary() != VK_SUCCESS)
			exit(-1);
		if (vhLoadExportedEntryPoints() != VK_SUCCESS)
			exit(-1);
		if (vhLoadGlobalLevelEntryPoints() != VK_SUCCESS)
			exit(-1);
	}

	//-------------------------------------------------------------------------------------------------------
	//init the engine

	/**
		* \brief Initialize the engine
		*
		* Creates a VEREnderer, VESceneManager, VEWindow. Then initializes Vulkan and gets the Vulkan instance.
		* After this, initializes VERenderer, VESceneManager and VEWindow. Then it registers default event listeners.
		*/
	void VEEngine::initEngine()
	{
		createRenderer(); //create a renderer
		createSceneManager(); //create a scene manager
		createWindow(); //create a window
		m_pWindow->initWindow(800, 600); //initialize the window

		m_threadPool = new ThreadPool(0); //worker threads

		std::vector<const char *> instanceExtensions = getRequiredInstanceExtensions();
		std::vector<const char *> validationLayers = getValidationLayers();
		vh::vhDevCreateInstance(instanceExtensions, validationLayers, &m_instance);
		if (vhLoadInstanceLevelEntryPoints(m_instance) != VK_SUCCESS)
			exit(-1);
		if (m_debug)
			vh::vhSetupDebugCallback(m_instance, &callback); //create a debug callback for printing debug information

		m_pWindow->createSurface(m_instance, &m_pRenderer->m_surface); //create a Vulkan surface
		m_pRenderer->initRenderer(); //initialize the renderer
		m_pSceneManager->initSceneManager(); //initialize the scene manager

		for (uint32_t i = veEvent::VE_EVENT_NONE; i < veEvent::VE_EVENT_LAST; i++)
		{ //initialize the event listeners lists
			m_eventListeners[(veEvent::veEventType)i] = new std::vector<VEEventListener *>;
		}
		m_loopCount = 1;
	}

	/**
		* \brief Create the only VEWindow instance and store a pointer to it.
		*
		* VEWindow is a base class only and should not be created itself. Instead, a derived class should be
		* instanciated, being tailored to one specific windowing system.
		*
		*/
	void VEEngine::createWindow()
	{
		m_pWindow = new VEWindowGLFW();
	}

	/**
		* \brief Create the only VERenderer instance and store a pointer to it.
		*
		* VERenderer is a base class only and should not be created itself. Instead, a derived class should be
		* instanciated, being tailored to one specific windowing system.
		*
		*/
	void VEEngine::createRenderer()
	{
		switch (m_rendererType)
		{
		case veRendererType::VE_RENDERER_TYPE_FORWARD:
			m_pRenderer = new VERendererForward();
			break;
		case veRendererType::VE_RENDERER_TYPE_DEFERRED:
			m_pRenderer = new VERendererDeferred();
			break;
		case veRendererType::VE_RENDERER_TYPE_RAYTRACING_NV:
			m_pRenderer = new VERendererRayTracingNV();
			break;
		case veRendererType::VE_RENDERER_TYPE_RAYTRACING_KHR:
			m_pRenderer = new VERendererRayTracingKHR();
			break;
		default:
			m_pRenderer = new VERendererForward();
		}
	}

	/**
		* \brief Create the only VESceneManager instance and store a pointer to it.
		*/
	void VEEngine::createSceneManager()
	{
		m_pSceneManager = new VESceneManager();
	}

	/**
		*
		* Register default event listener for steering the standard camera
		* overload if you do not want this event listener to be registered.
		*
		*/
	void VEEngine::registerEventListeners()
	{
		registerEventListener(new VEEventListenerGLFW("StandardEventListener")); //register a standard listener
	}

	/**
		* \brief Close the engine, delete all resources.
		*/
	void VEEngine::closeEngine()
	{
		//m_threadPool->shutdown();
		delete m_threadPool;

		clearEventListenerList();
		m_eventlist.clear();

		for (uint32_t i = veEvent::VE_EVENT_NONE; i < veEvent::VE_EVENT_LAST; i++)
		{ //initialize the event listeners lists
			delete m_eventListeners[(veEvent::veEventType)i];
		}

		m_pSceneManager->closeSceneManager();
		m_pRenderer->closeRenderer();
		m_pWindow->closeWindow();

		if (m_debug)
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
	std::vector<const char *> VEEngine::getValidationLayers()
	{
		std::vector<const char *> validationLayers = {};
		if (m_debug)
		{
			validationLayers.push_back("VK_LAYER_KHRONOS_validation");
			validationLayers.push_back("VK_LAYER_KHRONOS_monitor");
		}
		return validationLayers;
	}

	/**
		* \returns a list of Vulkan API instance extensions that are required.
		*/
	std::vector<const char *> VEEngine::getRequiredInstanceExtensions()
	{
		std::vector<const char *> extensions = m_pWindow->getRequiredInstanceExtensions();
		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		if (m_debug)
		{
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
	void VEEngine::registerEventListener(VEEventListener *pListener)
	{
		std::vector<veEvent::veEventType> eventTypes;
		for (uint32_t i = veEvent::VE_EVENT_NONE + 1; i < veEvent::VE_EVENT_LAST; i++)
		{
			eventTypes.push_back((veEvent::veEventType)i);
		}
		registerEventListener(pListener, eventTypes);
	};

	/**
		*
		* \brief Register an event listener for a specific evennt type
		*
		* The event listener will be sent events of this type
		*
		* \param[in] pListener Pointer to the event listener to be registered.
		* \param[in] eventTypes List with event types that are sent to this listener
		*
		*/
	void VEEngine::registerEventListener(VEEventListener *pListener, std::vector<veEvent::veEventType> eventTypes)
	{
		for (auto eventType : eventTypes)
		{
			m_eventListeners[eventType]->push_back(pListener);
		}
	}

	/**
		*
		* \brief Return a pointer to a specified event listener
		*
		* \param[in] name Name of the listener that should be found
		* \returns a pointer to the event listener having this name
		*
		*/
	VEEventListener *VEEngine::getEventListener(std::string name)
	{
		for (auto list : m_eventListeners)
		{
			for (auto listener : *(list.second))
			{
				if (listener->getName() == name)
				{
					return listener;
				}
			}
		}
		return nullptr;
	}

	/**
		*
		* \brief Register an event listener for a specific evennt type
		*
		* The event listener will be sent events of this type
		*
		* \param[in] name Name of the listener that should be removed
		* \param[in] list Reference to the list that this listener should be registered in
		*
		*/
	void VEEngine::removeEventListener(std::string name, std::vector<VEEventListener *> *list)
	{
		for (uint32_t i = 0; i < list->size(); i++)
		{
			if ((*list)[i]->getName() == name)
			{
				(*list)[i] = (*list)[list->size() - 1]; //write over last listener
				list->pop_back();
				return;
			}
		}
	}

	/**
		*
		* \brief Remove an event listener.
		*
		* The event listener will be removed fr�m the list but not destroyed.
		*
		* \param[in] name The name of the event listener to be removed.
		*
		*/
	void VEEngine::removeEventListener(std::string name)
	{
		for (auto list : m_eventListeners)
		{
			removeEventListener(name, list.second);
		}
	};

	/**
		*
		* \brief Delete an event listener.
		*
		* The event listener will be removed from the list of listeners and be destroyed
		*
		* \param[in] name The name of the listener zu be destroyed
		*
		*/
	void VEEngine::deleteEventListener(std::string name)
	{
		VEEventListener *pListener = getEventListener(name);
		if (pListener != nullptr)
		{
			removeEventListener(name);
			delete pListener;
		}
	};

	/**
		*
		* \brief Destroy all event listeners
		*
		*/
	void VEEngine::clearEventListenerList()
	{
		std::set<VEEventListener *> lset;

		for (uint32_t i = veEvent::VE_EVENT_NONE; i < veEvent::VE_EVENT_LAST; i++)
		{
			//initialize the event listeners lists
			for (uint32_t j = 0; j < m_eventListeners[(veEvent::veEventType)i]->size(); j++)
			{
				lset.insert((*m_eventListeners[(veEvent::veEventType)i])[j]);
			}
			m_eventListeners[(veEvent::veEventType)i]->clear();
		}
		for (auto pListener : lset)
			delete pListener;
	}

	/**
		*
		* \brief Call all listeners registered for a specific event type
		*
		* \param[in] dt Delta time that passed by since the last loop.
		* \param[in] event The event that should be passed to all listeners.
		*
		*/
	void VEEngine::callListeners(double dt, veEvent event)
	{
		callListeners(dt, event, m_eventListeners[event.type]);
	}

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
		* \param[in] list The list of registered event listeners, that should receive this event
		*
		*/
	void VEEngine::callListeners(double dt, veEvent event, std::vector<VEEventListener *> *list)
	{
		static std::vector<std::future<void>> futures;
		event.dt = dt;

		const uint32_t granularity = 200;
		if (m_threadPool->threadCount() > 1 && list->size() > granularity)
		{
			uint32_t numThreads = std::min((int)(list->size() / granularity), (int)m_threadPool->threadCount());
			uint32_t numListenerPerThread = (uint32_t)list->size() / numThreads;
			futures.resize(numThreads);

			uint32_t startIdx, endIdx;
			for (uint32_t k = 0; k < numThreads; k++)
			{
				startIdx = k * numListenerPerThread;
				endIdx = k == numThreads - 1 ? (uint32_t)list->size() - 1 : (k + 1) * numListenerPerThread - 1;

				auto future = m_threadPool->add(&VEEngine::callListeners2, this, dt, event, list, startIdx, endIdx);
				futures[k] = std::move(future);
			}
			for (uint32_t i = 0; i < futures.size(); i++)
				futures[i].get();
		}
		else
		{
			if (list->size() > 0)
				callListeners2(dt, event, list, 0, (uint32_t)list->size() - 1);
		}
	}

	/**
		*
		* \brief Call a subset of the listeners, which can be done in parallel
		*
		* \param[in] dt Delta time that passed by since the last loop.
		* \param[in] event The event that should be processed.
		* \param[in] list The list of registered event listeners, that should receive this event
		* \param[in] startIdx Index of the first listener to call
		* \param[in] endIdx Index of the last listener to call
		*
		*/
	void VEEngine::callListeners2(double dt, veEvent event, std::vector<VEEventListener *> *list, uint32_t startIdx, uint32_t endIdx)
	{
		for (uint32_t i = startIdx; i <= endIdx; i++)
		{
			if ((*list)[i]->onEvent(event))
				return; //if return true then the listener has consumed the event, so stop processing it
		}
	}

	//---------------------------------------------------------------------------------------------------

	/**
		*
		* \brief Add an event to the event list.
		*
		* This adds a new event that should be processed in the next iteration.
		*
		* \param[in] event The event that should be processed.
		*
		*/
	void VEEngine::addEvent(veEvent event)
	{
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
	void VEEngine::deleteEvent(veEvent event)
	{
		for (uint32_t i = 0; i < m_eventlist.size(); i++)
		{
			if (event.type == m_eventlist[i].type &&
				event.subsystem == m_eventlist[i].subsystem &&
				//event.idata1 == m_eventlist[i].idata1 &&
				event.idata2 == m_eventlist[i].idata2 //&&
				//event.idata3 == m_eventlist[i].idata3 &&
				//event.idata4 == m_eventlist[i].idata4
				)
			{
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
	void VEEngine::processEvents(double dt)
	{
		std::vector<veEvent> keepEvents; //Keep these events in the list
		for (auto event : m_eventlist)
		{
			if (event.lifeTime == veEvent::VE_EVENT_LIFETIME_CONTINUOUS)
			{
				keepEvents.push_back(event);
			}
		}

		for (auto event : m_eventlist)
		{
			if (event.notBeforeTime <= m_loopCount)
			{
				callListeners(dt, event);
			}
		}
		m_eventlist = keepEvents;
	}

	/**
		*
		* \brief Inform the engine that the window size has been changed.
		*
		* If the window size has been changed, then the renderer has to change many resources like the swap chain size,
		* framebuffers etc.
		*
		*/
	void VEEngine::windowSizeChanged()
	{
		m_framebufferResized = true;
	};

	//-------------------------------------------------------------------------------------------------------
	//getter functions

	/**
		* \returns the Vulkan instance.
		*/
	VkInstance VEEngine::getInstance()
	{
		return m_instance;
	}

	/**
		* \returns the VEWindow instance.
		*/
	VEWindow *VEEngine::getWindow()
	{
		return m_pWindow;
	}

	/**
		* \returns the VESceneManager instance.
		*/
	VESceneManager *VEEngine::getSceneManager()
	{
		return m_pSceneManager;
	}

	/**
		* \returns the VERenderer instance.
		*/
	VERenderer *VEEngine::getRenderer()
	{
		return m_pRenderer;
	}

	veRendererType VEEngine::getRendererType()
	{
		return m_rendererType;
	}

	/**
		* \returns the loop counter.
		*/
	uint32_t VEEngine::getLoopCount()
	{
		return m_loopCount;
	}

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
	void VEEngine::run() 
	{
		std::chrono::high_resolution_clock::time_point t_start = vh::vhTimeNow();
		std::chrono::high_resolution_clock::time_point t_prev = t_start;
		std::chrono::high_resolution_clock::time_point t_now;

		while ( !m_end_running) {
			m_dt = vh::vhTimeDuration( t_prev );
			m_AvgFrameTime = vh::vhAverage( (float)m_dt, m_AvgFrameTime );
			t_prev = vh::vhTimeNow();

			//----------------------------------------------------------------------------------
			//process frame begin

			t_now = vh::vhTimeNow();
			veEvent event(veEvent::VE_EVENT_FRAME_STARTED );	//notify all listeners that a new frame starts
			callListeners(m_dt, event);
			m_AvgStartedTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgStartedTime);

			//----------------------------------------------------------------------------------
			//get and process window events

			m_pWindow->pollEvents();			//poll window events which should be injected into the event queue

			if (m_framebufferResized) {			//if window size changed, recreate the current swapchain
				m_pWindow->waitForWindowSizeChange();
				m_pRenderer->recreateSwapchain();
				m_framebufferResized = false;
			}
			
			t_now = vh::vhTimeNow();
			processEvents(m_dt);				//process all current events, including pressed keys
			m_AvgEventTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgEventTime);

			//----------------------------------------------------------------------------------
			//update world matrices and send them to the GPU

			t_now = vh::vhTimeNow();
			getSceneManagerPointer()->updateSceneNodes(getEnginePointer()->getRenderer()->getImageIndex());	//update scene node UBOs
			m_AvgUpdateTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgUpdateTime);

			//----------------------------------------------------------------------------------
			//submit the cmd buffer to the GPU

			t_now = vh::vhTimeNow();
			m_pRenderer->drawFrame();			//draw the next frame
			m_AvgDrawTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgDrawTime);

			//----------------------------------------------------------------------------------
			//Overlay

			t_now = vh::vhTimeNow();
			m_pRenderer->prepareOverlay();
			m_AvgPrepOvlTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgPrepOvlTime);

			t_now = vh::vhTimeNow();
			event.type = veEvent::VE_EVENT_DRAW_OVERLAY;		//notify all listeners that they can draw an overlay now
			callListeners(m_dt, event);
			m_AvgEndedTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgEndedTime);

			t_now = vh::vhTimeNow();
			m_pRenderer->drawOverlay();			//draw overlay in the subrenderer
			m_AvgDrawOvlTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgDrawOvlTime);

			//----------------------------------------------------------------------------------
			//Frame ended

			t_now = vh::vhTimeNow();
			event.type = veEvent::VE_EVENT_FRAME_ENDED;	//notify all listeners that the frame ended, e.g. fill cmd buffers for overlay
			callListeners(m_dt, event);
			m_AvgEndedTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgEndedTime);

			//----------------------------------------------------------------------------------
			//present the finished frame

			t_now = vh::vhTimeNow();
			m_pRenderer->presentFrame();		//present the next frame
			m_AvgPresentTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgPresentTime);

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
	void VEEngine::end()
	{
		m_end_running = true;
	}

	///\brief load standard level with standard camera and lights
	void VEEngine::loadLevel(uint32_t numLevel)
	{

		//camera parent is used for translations
		VESceneNode *cameraParent = getSceneManagerPointer()->createSceneNode("StandardCameraParent", getRoot(), 
																			  glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

		//camera can only do yaw (parent y-axis) and pitch (local x-axis) rotations
		VkExtent2D extent = getWindowPointer()->getExtent();
		VECameraProjective *camera = (VECameraProjective *)getSceneManagerPointer()->createCamera("StandardCamera", VECamera::VE_CAMERA_TYPE_PROJECTIVE, cameraParent);
		camera->m_nearPlane = 0.1f;
		camera->m_farPlane = 500.1f;
		camera->m_aspectRatio = extent.width / (float)extent.height;
		camera->m_fov = 45.0f;
		camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		getSceneManagerPointer()->setCamera(camera);

		VELight *light4 = (VESpotLight *)getSceneManagerPointer()->createLight("StandardAmbientLight", VELight::VE_LIGHT_TYPE_AMBIENT, camera);
		light4->m_col_ambient = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

		//use one light source
		VELight *light1 = (VEDirectionalLight *)getSceneManagerPointer()->createLight("StandardDirLight", VELight::VE_LIGHT_TYPE_DIRECTIONAL, getRoot());     //new VEDirectionalLight("StandardDirLight");
		light1->lookAt(glm::vec3(0.0f, 20.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		light1->m_col_diffuse = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
		light1->m_col_specular = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
		
		/*VELight *light3 = (VEPointLight *)getSceneManagerPointer()->createLight("StandardPointLight", VELight::VE_LIGHT_TYPE_POINT, camera); //new VEPointLight("StandardPointLight");		//sphere is attached to this!
		light3->m_col_diffuse = glm::vec4(0.99f, 0.99f, 0.6f, 1.0f);
		light3->m_col_specular = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		light3->m_param[0] = 200.0f;
		light3->multiplyTransform(glm::translate(glm::vec3(0.0f, 0.0f, 15.0f)));
		VELight *light2 = (VESpotLight *)getSceneManagerPointer()->createLight("StandardSpotLight", VELight::VE_LIGHT_TYPE_SPOT, camera);  
		light2->m_col_diffuse = glm::vec4(0.99f, 0.6f, 0.6f, 1.0f);
		light2->m_col_specular = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		light2->m_param[0] = 200.0f;
		light2->multiplyTransform(glm::translate(glm::vec3(5.0f, 0.0f, 0.0f)));
		*/
	
		registerEventListeners();
	}

} // namespace ve
