#pragma once


namespace vve {

	//-------------------------------------------------------------------------------------------------------
	//Camera

	/**
	 * @brief Camera structure with projection parameters
	 */
	struct Camera {
		real_t m_aspect = 16.0f / 9.0f;
		real_t m_near = 0.1f;
		real_t m_far = 1000.0f;
		real_t m_fov = 45.0f;
		/**
		 * @brief Calculate projection matrix
		 * @return Projection matrix for this camera
		 */
		auto Matrix() -> mat4_t { mat4_t proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far); proj[1][1] *= -1; return proj; }
	};

	//-------------------------------------------------------------------------------------------------------

    /**
     * @brief Manages the scene graph and scene nodes
     */
    class SceneManager : public System {

    public:

		inline static const std::string m_windowName = "VVE Window";
		inline static const std::string m_worldName = "VVE WorldSceneNode";
		inline static const std::string m_rootName = "VVE RootSceneNode";
		inline static const std::string m_cameraName = "VVE Camera";
		inline static const std::string m_cameraNodeName = "VVE CameraNode";

        /**
         * @brief Constructor for SceneManager
         * @param systemName Name of the system
         * @param engine Reference to the engine
         */
        SceneManager(std::string systemName, Engine& engine );
        /**
         * @brief Destructor for SceneManager
         */
        virtual ~SceneManager();

    private:
		bool OnInit(Message message);
		bool OnLoadLevel(Message message);
		bool OnWindowSize(Message message);
		bool OnUpdate(Message message);
		bool OnSceneCreate(Message message);
		bool OnObjectCreate(Message message);
		void ProcessNode(aiNode* node, ParentHandle parent, std::filesystem::path& filepath, const aiScene* scene, uint64_t& id);
		bool OnObjectSetParent(Message message);
		void SetParent(ObjectHandle object, ParentHandle parent);
		bool OnObjectDestroy(Message message);

		//std::shared_mutex m_mutex;
		CameraHandle m_cameraHandle;
		ObjectHandle m_cameraNodeHandle;
		ObjectHandle m_worldHandle;
		ObjectHandle m_rootHandle;
    };

};  // namespace vve





