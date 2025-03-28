#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	GUI::GUI(std::string systemName, Engine& engine, std::string windowName ) : 
		System(systemName, engine), m_windowName(windowName) {
		m_engine.RegisterCallbacks( { 
			{this, 0, "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this, 0, "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyDown(message);} },
			{this, 0, "SDL_KEY_UP", [this](Message& message){ return OnKeyUp(message);} },
			{this, 0, "SDL_MOUSE_BUTTON_DOWN", [this](Message& message){ return OnMouseButtonDown(message);} },
			{this, 0, "SDL_MOUSE_BUTTON_UP", [this](Message& message){return OnMouseButtonUp(message);} },
			{this, 0, "SDL_MOUSE_MOVE", [this](Message& message){ return OnMouseMove(message); } },
			{this, 0, "SDL_MOUSE_WHEEL", [this](Message& message){ return OnMouseWheel(message); } },
			{this, 0, "FRAME_END", [this](Message& message){ return OnFrameEnd(message); } }
		} );
	};


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

		if( key == SDL_SCANCODE_O  ) { m_makeScreenshot = true; return false; }
		if( key == SDL_SCANCODE_P  ) { m_makeScreenshotDepth = true; return false; }

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

		float speed = m_shiftPressed ? 20.0f : 10.0f; ///add the new translation vector to the previous one
		pn() = pn() + translate * (real_t)dt * speed;

		float rotSpeed = m_shiftPressed ? 2.0f : 1.0f;
		angle1 = rotSpeed * (float)dt * -dx; //left right
		axis1 = glm::vec3(0.0, 0.0, 1.0);
		rn() = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rn() } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc() = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc() } };

		glm::vec4 ctopp = LtoPc()[3];
		glm::vec4 ptppp = LtoPn()[3];
		//std::cout << "Camera Parent: (" << pn().x << ", " << pn().y << ", " << pn().z << ") " <<
	    //		        " Camera PT: (" << ptppp.x << ", " << ptppp.y << ", " << ptppp.z << ") " << std::endl;

		//glm::vec4 test = LtoPc() * vec4_t{0.0f, 0.0f, -1.0f, 0.0f};
		//std::cout << "Test: (" << test.x << ", " << test.y << ", " << test.z << ") " << std::endl;

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
		rn() = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rn() } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc() = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc() } };

		return false;
	}

	
	bool GUI::OnMouseWheel(Message message) {
		GetCamera();
		auto msg = message.template GetData<MsgMouseWheel>();
		real_t dt = (real_t)msg.m_dt;
		auto [pn, rn, sn, LtoPn] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		
		float speed = m_shiftPressed ? 500.0f : 100.0f; ///add the new translation vector to the previous one
		auto translate = vec3_t{ LtoPn() * LtoPc() * vec4_t{0.0f, 0.0f, -1.0f, 0.0f} };
		pn() = pn() + translate * (real_t)dt * (real_t)msg.m_y * speed;
		return false;
	}

	void GUI::GetCamera() {
		if(m_cameraHandle.IsValid() == false) { 
			auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, Camera&, ParentHandle>().begin(); 
			m_cameraHandle = handle;
			m_cameraNodeHandle = parent;
		};
	}


	bool GUI::OnFrameEnd(Message message) {
		if (m_makeScreenshot) {
			auto vstate = std::get<1>(Renderer::GetState(m_registry));
			auto wstate = std::get<1>(Window::GetState(m_registry, m_windowName));

			VkExtent2D extent = { (uint32_t)wstate().m_width, (uint32_t)wstate().m_height };
			uint32_t imageSize = extent.width * extent.height * 4;
			VkImage image = vstate().m_swapChain.m_swapChainImages[vstate().m_imageIndex]; 

			uint8_t *dataImage = new uint8_t[imageSize];

			vh::ImgCopyImageToHost( vstate().m_device, vstate().m_vmaAllocator, vstate().m_graphicsQueue,
				vstate().m_commandPool, image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				dataImage, extent.width, extent.height, imageSize, 2, 1, 0, 3);

			m_numScreenshot++;

			std::string name("screenshots/screenshot" + std::to_string(m_numScreenshot - 1) + ".jpg");
			stbi_write_jpg(name.c_str(), extent.width, extent.height, 4, dataImage, 4 * extent.width);
			delete[] dataImage;

			m_makeScreenshot = false;
		}

		/*
		if (m_makeScreenshotDepth) {

			VETexture *map = getRendererShaderPointer()->getShadowMap(getRendererPointer()->getImageIndex())[0];
			//VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkExtent2D extent = map->m_extent;
			uint32_t imageSize = extent.width * extent.height;
			VkImage image = map->m_image;

			float *dataImage = new float[imageSize];
			unsigned char*dataImage2 = new unsigned char[imageSize];

			vh::vhBufCopyImageToHost(getRendererPointer()->getDevice(),
				getRendererPointer()->getVmaAllocator(),
				getRendererPointer()->getGraphicsQueue(),
				getRendererPointer()->getCommandPool(),
				image, map->m_format, VK_IMAGE_ASPECT_DEPTH_BIT, layout,
				(unsigned char*)dataImage, extent.width, extent.height, imageSize * 4);

			for (uint32_t v = 0; v < extent.height; v++) {
				for (uint32_t u = 0; u < extent.width; u++) {
					dataImage2[v*extent.width + u] = (unsigned char)((dataImage[v*extent.width + u]-0.5)*256.0f*2.0f);
					//std::cout << dataImage[v*extent.width + u] << " ";
				}
			}

			std::string name("../../media/screenshots/screenshot" + std::to_string(m_numScreenshot) + ".jpg");
			stbi_write_jpg(name.c_str(), extent.width, extent.height, 1, dataImage2, extent.width);
			delete[] dataImage;
			delete[] dataImage2;

			m_numScreenshot++;
			m_makeScreenshotDepth = false;
			}
		*/
		return false;
	};


};  // namespace vve

