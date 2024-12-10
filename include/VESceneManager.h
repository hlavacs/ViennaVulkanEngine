#pragma once

#include "VEInclude.h"
#include "VESystem.h"
#include "VERendererVulkan.h"
#include "VECS.h"

namespace vve {

	struct MsgTextureCreate : public MsgBase { 
		MsgTextureCreate(void* s, void* r, void *pixels, vecs::Handle handle); 
		void* m_pixels; vecs::Handle m_handle; 
	};

    struct MsgTextureDestroy : public MsgBase { MsgTextureDestroy(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };

    struct MsgGeometryCreate : public MsgBase { MsgGeometryCreate(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };
    struct MsgGeometryDestroy : public MsgBase { MsgGeometryDestroy(void* s, void* r, vecs::Handle handle); vecs::Handle m_handle; };


  	template<ArchitectureType ATYPE>
    class SceneManager : public System<ATYPE> {

		using System<ATYPE>::m_engine;

		struct Transform {
			vec3_t m_position{};
			quat_t m_rotation{};
			vec3_t m_scale{};
			auto GetMatrix() -> mat4_t;
		};

		struct SceneNode {
			std::string m_name;
			Transform m_transform;
			size_t m_parent;
			std::vector<size_t> m_children;
			std::vector<vecs::Handle> m_objects;
		};

    public:
        SceneManager(std::string systemName, Engine<ATYPE>* engine );
        virtual ~SceneManager();
		auto LoadTexture(std::string filename)-> vecs::Handle;
		auto LoadOBJ(std::string filename) -> vecs::Handle;
		auto LoadGLTF(std::string filename) -> vecs::Handle;

		auto GetAsset(std::string filename) -> vecs::Handle;

    private:

		std::unordered_map<std::string, vecs::Handle> m_files;

    };

};  // namespace vve

