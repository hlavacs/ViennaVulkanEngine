#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	GUI::GUI(std::string systemName, Engine& engine, std::string windowName ) : 
		System(systemName, engine), m_windowName(windowName) {
		m_engine.RegisterCallback( { 
 		  	{this, 0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} }, 
			{this, 0, "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this, 0, "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyDown(message);} },
			{this, 0, "SDL_KEY_UP", [this](Message& message){ return OnKeyUp(message);} },
			{this, 0, "SDL_MOUSE_BUTTON_DOWN", [this](Message& message){ return OnMouseButtonDown(message);} },
			{this, 0, "SDL_MOUSE_BUTTON_UP", [this](Message& message){return OnMouseButtonUp(message);} },
			{this, 0, "SDL_MOUSE_MOVE", [this](Message& message){ return OnMouseMove(message); } },
			{this, 0, "SDL_MOUSE_WHEEL", [this](Message& message){ return OnMouseWheel(message); } }
		} );
	};

    bool GUI::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == m_windowName ) {
			m_windowSDL = (WindowSDL*)msg.m_sender;
		}
		return false;
	}

	bool GUI::OnKeyDown(Message message) {
		GetCamera();

		int key;
		real_t dt;
		if(message.HasType<MsgKeyDown>()) {
			auto msg = message.template GetData<MsgKeyDown>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		} else {
			auto msg = message.template GetData<MsgKeyRepeat>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		}

		if( key == SDL_SCANCODE_ESCAPE  ) { m_engine.Stop(); return false; }
		if( key == SDL_SCANCODE_LSHIFT || key == SDL_SCANCODE_RSHIFT  ) { m_shiftPressed = true; return false; }

		auto [pn, rn, sn, LtoPn] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
	
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis1 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle1 = 0.0f;
		vec3_t axis2 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle2 = 0.0f;

		int dx{0}, dy{0};
		switch( key )  {
			case SDL_SCANCODE_W : { translate = vec3_t{ LtoPn() * LtoPc() * vec4_t{0.0f, 0.0f, -1.0f, 0.0f} }; break; }
			case SDL_SCANCODE_S : { translate = vec3_t{ LtoPn() * LtoPc() * vec4_t{0.0f, 0.0f, 1.0f, 0.0f} }; break; }
			case SDL_SCANCODE_A : { translate = vec3_t{ LtoPn() * LtoPc() * vec4_t{-1.0f, 0.0f, 0.0f, 0.0f} }; break; }
			case SDL_SCANCODE_D : { translate = vec3_t{ LtoPn() * LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} }; break; }
			case SDL_SCANCODE_Q : { translate = vec3_t(0.0f, 0.0f, -1.0f); break; }
			case SDL_SCANCODE_E : { translate = vec3_t(0.0f, 0.0f, 1.0f); break; }
			case SDL_SCANCODE_LEFT : { dx=-1; break; }
			case SDL_SCANCODE_RIGHT : { dx=1; break; }
			case SDL_SCANCODE_UP : { dy=1; break; }
			case SDL_SCANCODE_DOWN : { dy=-1; break; }
		}

		float speed = m_shiftPressed ? 30.0f : 2.0f; ///add the new translation vector to the previous one
		pn = pn() + translate * (real_t)dt * speed;

		float rotSpeed = m_shiftPressed ? 2.0f : 1.0f;
		angle1 = rotSpeed * (float)dt * -dx; //left right
		axis1 = glm::vec3(0.0, 0.0, 1.0);
		rn = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rn } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc } };

		return false;
    }

	bool GUI::OnKeyUp(Message message) {
		auto msg = message.template GetData<MsgKeyUp>();
		int key = msg.m_key;
		if( key == SDL_SCANCODE_LSHIFT || key == SDL_SCANCODE_RSHIFT ) { m_shiftPressed = false; }
		return false;
	}

	bool GUI::OnMouseButtonDown(Message message) {
		auto msg = message.template GetData<MsgMouseButtonDown>();
		if(msg.m_button != SDL_BUTTON_RIGHT) return false;
 		m_mouseButtonDown = true; 
		m_x = m_y = -1; 
		return false;
	}

	bool GUI::OnMouseButtonUp(Message message) {
		auto msg = message.template GetData<MsgMouseButtonUp>();
		if(msg.m_button != SDL_BUTTON_RIGHT) return false;
		m_mouseButtonDown = false; 
		return false;
	}


	bool GUI::OnMouseMove(Message message) {
		if( m_mouseButtonDown == false ) return false;
		GetCamera();

		auto msg = message.template GetData<MsgMouseMove>();
		real_t dt = (real_t)msg.m_dt;
		if( m_x==-1 ) { m_x = msg.m_x; m_y = msg.m_y; }
		int dx = msg.m_x - m_x;
		m_x = msg.m_x;
		int dy = msg.m_y - m_y;
		m_y = msg.m_y;

		auto [pn, rn, sn] = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis1 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle1 = 0.0f;
		vec3_t axis2 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle2 = 0.0f;

		float rotSpeed = m_shiftPressed ? 1.0f : 0.5f;
		angle1 = rotSpeed * (float)dt * -dx; //left right
		axis1 = glm::vec3(0.0, 0.0, 1.0);
		rn = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rn } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc } };

		return false;
	}

	
	bool GUI::OnMouseWheel(Message message) {
		GetCamera();
		auto msg = message.template GetData<MsgMouseWheel>();
		real_t dt = (real_t)msg.m_dt;
		auto [pn, rn, sn] 		 = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		float speed = m_shiftPressed ? 2000.0f : 500.0f; ///add the new translation vector to the previous one
		pn = pn() + mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, -msg.m_y) * (real_t)dt * speed;
		return false;
	}

	void GUI::GetCamera() {
		if(m_cameraHandle.IsValid() == false) { 
			auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, Camera&, ParentHandle>().begin(); 
			m_cameraHandle = handle;
			m_cameraNodeHandle = parent;
		};
	}

};  // namespace vve

