/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	/**
		*
		* \brief Callback for GLFW keyboard events
		*
		* \param[in] event The keyboard event
		* \returns true to consume the event
		*/
	bool VEEventListenerGLFW::onKeyboard(veEvent event)
	{
		if (event.idata1 == GLFW_KEY_ESCAPE)
		{ //ESC pressed - end the engine
			getEnginePointer()->end();
			return true;
		}

		if (event.idata3 == GLFW_RELEASE)
			return false;

		if (event.idata1 == GLFW_KEY_P && event.idata3 == GLFW_PRESS)
		{
			m_makeScreenshot = true;
			return false;
		}
		if (event.idata1 == GLFW_KEY_O && event.idata3 == GLFW_PRESS)
		{
			m_makeScreenshotDepth = true;
			return false;
		}

		///create some default constants for the actions
		glm::vec4 translate = glm::vec4(0.0, 0.0, 0.0, 1.0); //total translation
		glm::vec4 rot4 = glm::vec4(1.0); //total rotation around the axes, is 4d !
		float angle = 0.0;
		float rotSpeed = 2.0;

		VECamera *pCamera = getSceneManagerPointer()->getCamera();
		VESceneNode *pParent = pCamera->getParent();

		switch (event.idata1)
		{
		case GLFW_KEY_A:
			translate = pCamera->getTransform() * glm::vec4(-1.0, 0.0, 0.0, 1.0); //left
			break;
		case GLFW_KEY_D:
			translate = pCamera->getTransform() * glm::vec4(1.0, 0.0, 0.0, 1.0); //right
			break;
		case GLFW_KEY_W:
			translate = pCamera->getTransform() * glm::vec4(0.0, 0.0, 1.0, 1.0); //forward
			//translate.y = 0.0f;
			break;
		case GLFW_KEY_S:
			translate = pCamera->getTransform() * glm::vec4(0.0, 0.0, -1.0, 1.0); //back
			translate.y = 0.0f;
			break;
		case GLFW_KEY_Q:
			translate = glm::vec4(0.0, -1.0, 0.0, 1.0); //down
			break;
		case GLFW_KEY_E:
			translate = glm::vec4(0.0, 1.0, 0.0, 1.0); //up
			break;
		case GLFW_KEY_LEFT: //yaw rotation is already in parent space
			angle = rotSpeed * (float)event.dt * -1.0f;
			rot4 = glm::vec4(0.0, 1.0, 0.0, 1.0);
			break;
		case GLFW_KEY_RIGHT: //yaw rotation is already in parent space
			angle = rotSpeed * (float)event.dt * 1.0f;
			rot4 = glm::vec4(0.0, 1.0, 0.0, 1.0);
			break;
		case GLFW_KEY_UP: //pitch rotation is in cam/local space
			angle = rotSpeed * (float)event.dt * 1.0f; //pitch angle
			rot4 = pCamera->getTransform() * glm::vec4(1.0, 0.0, 0.0, 1.0); //x axis from local to parent space!
			break;
		case GLFW_KEY_DOWN: //pitch rotation is in cam/local space
			angle = rotSpeed * (float)event.dt * -1.0f; //pitch angle
			rot4 = pCamera->getTransform() * glm::vec4(1.0, 0.0, 0.0, 1.0); //x axis from local to parent space!
			break;

		default:
			return false;
		};

		if (pParent == nullptr)
		{
			pParent = pCamera;
		}

		///add the new translation vector to the previous one
		float speed = 6.0f;
		glm::vec3 trans = speed * glm::vec3(translate.x, translate.y, translate.z);
		pParent->multiplyTransform(glm::translate(glm::mat4(1.0f), (float)event.dt * trans));

		///combination of yaw and pitch, both wrt to parent space
		glm::vec3 rot3 = glm::vec3(rot4.x, rot4.y, rot4.z);
		glm::mat4 rotate = glm::rotate(glm::mat4(1.0), angle, rot3);
		pCamera->multiplyTransform(rotate);

		return true;
	}

	/**
		*
		* \brief Default behavior when the mouse is moved
		*
		* If left button is clicked then is is equivalent of UP/DOWN LEFT/RIGHT keys will rotate the camera.
		* For this we need the previous cursor position so we can determine how the mouse moved, and use this
		* information to move the engine camera.
		*
		* \param[in] event The mouse move event
		* \returns false so event is not consumed
		*
		*/
	bool VEEventListenerGLFW::onMouseMove(veEvent event)
	{
		if (!m_rightButtonClicked)
			return false; //only do something if left mouse button is pressed

		float x = event.fdata1;
		float y = event.fdata2;

		if (!m_usePrevCursorPosition)
		{ //can I use the previous cursor position ?
			m_cursorPrevX = x;
			m_cursorPrevY = y;
			m_usePrevCursorPosition = true;
			return true;
		}

		float dx = x - m_cursorPrevX; //motion of cursor in x and y direction
		float dy = y - m_cursorPrevY;

		m_cursorPrevX = x; //remember this for next iteration
		m_cursorPrevY = y;

		VECamera *pCamera = getSceneManagerPointer()->getCamera();
		VESceneNode *pParent = pCamera->getParent();

		float slow = 0.5; //camera rotation speed

		//dx
		float angledx = slow * (float)event.dt * dx;
		glm::vec4 rot4dx = glm::vec4(0.0, 1.0, 0.0, 1.0);
		glm::vec3 rot3dx = glm::vec3(rot4dx.x, rot4dx.y, rot4dx.z);
		glm::mat4 rotatedx = glm::rotate(glm::mat4(1.0), angledx, rot3dx);

		//dy
		float angledy = slow * (float)event.dt * dy; //pitch angle
		glm::vec4 rot4dy = pCamera->getTransform() * glm::vec4(1.0, 0.0, 0.0, 1.0); //x axis from local to parent space!
		glm::vec3 rot3dy = glm::vec3(rot4dy.x, rot4dy.y, rot4dy.z);
		glm::mat4 rotatedy = glm::rotate(glm::mat4(1.0), angledy, rot3dy);

		pCamera->multiplyTransform(rotatedx * rotatedy);

		return false;
	}

	/**
		*
		* \brief Track buttons of the mouse
		*
		* If a button is clicked or released then this is noted in the engine m_mouse_buttons_clicked set
		*
		* \param[in] event The mouse button event
		* \returns true (event is consumed) or false (event is not consumed)
		*
		*/
	bool VEEventListenerGLFW::onMouseButton(veEvent event)
	{
		if (event.idata3 == GLFW_PRESS)
		{ //just pressed a mouse button
			m_usePrevCursorPosition = false;
			if (event.idata1 == GLFW_MOUSE_BUTTON_RIGHT)
				m_rightButtonClicked = true;
			return true;
		}

		if (event.idata3 == GLFW_RELEASE)
		{ //just released a mouse button
			m_usePrevCursorPosition = false;
			if (event.idata1 == GLFW_MOUSE_BUTTON_RIGHT)
				m_rightButtonClicked = false;
			return true;
		}

		return false;
	}

	/**
		*
		* \brief React to mouse scroll events
		*
		* They are the same as key W/S
		*
		* \param[in] event The mouse scroll event
		* \returns false, so event is not consumed
		*
		*/
	bool VEEventListenerGLFW::onMouseScroll(veEvent event)
	{
		float xoffset = event.fdata1;
		float yoffset = event.fdata2;

		VECamera *pCamera = getSceneManagerPointer()->getCamera();
		VESceneNode *pParent = pCamera->getParent();
		glm::vec4 translate = 1000 * yoffset * glm::vec4(0.0, 0.0, 1.0, 1.0);

		if (pParent == nullptr)
		{
			pParent = pCamera;
		}
		else
		{
			//so far the translation vector was defined in cam local space. But the camera frame of reference
			//is defined wrt its parent space, so we must transform this vector to parent space
			translate = pCamera->getTransform() * translate; //transform from local camera space to parent space
		}

		//add the new translation vector to the previous one
		glm::vec3 trans = glm::vec3(translate.x, translate.y, translate.z);
		pParent->setTransform(glm::translate(pParent->getTransform(), (float)event.dt * trans));

		return false;
	}

	/**
		*
		* \brief Make a screenshot and save it as PNG
		*
		* If key P has been pressed, transfer the last swapchain image to the host and store it in a PNG
		*
		* \param[in] event The onFrameEnded event
		* \returns false, so event is not consumed
		*
		*/
	void VEEventListenerGLFW::onFrameEnded(veEvent event)
	{
		if (m_makeScreenshot)
		{
			VkExtent2D extent = getWindowPointer()->getExtent();
			uint32_t imageSize = extent.width * extent.height * 4;
			VkImage image = getEnginePointer()->getRenderer()->getSwapChainImage();

			uint8_t *dataImage = new uint8_t[imageSize];

			vh::vhBufCopySwapChainImageToHost(getEnginePointer()->getRenderer()->getDevice(),
				getEnginePointer()->getRenderer()->getVmaAllocator(),
				getEnginePointer()->getRenderer()->getGraphicsQueue(),
				getEnginePointer()->getRenderer()->getCommandPool(),
				image, VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				dataImage, extent.width, extent.height, imageSize);

			m_numScreenshot++;

			std::string name("media/screenshots/screenshot" + std::to_string(m_numScreenshot - 1) + ".jpg");
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

				std::string name("media/screenshots/screenshot" + std::to_string(m_numScreenshot) + ".jpg");
				stbi_write_jpg(name.c_str(), extent.width, extent.height, 1, dataImage2, extent.width);
				delete[] dataImage;
				delete[] dataImage2;

				m_numScreenshot++;
				m_makeScreenshotDepth = false;
			}
			*/
	};

} // namespace ve
