/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION

#include "VEInclude.h"

namespace ve
{
	/**
		* \brief Initialize the subrenderer
		*
		* Create descriptor set layout, pipeline layout and the PSO
		*
		*/
	void VESubrender_Nuklear::initSubrenderer()
	{
		VESubrender::initSubrenderer();

		struct nk_font_atlas *atlas;

		vh::QueueFamilyIndices queueFamilyIndices =
			vh::vhDevFindQueueFamilies(m_renderer.getPhysicalDevice(), m_renderer.getSurface());

		m_ctx = nk_glfw3_init(((VEWindowGLFW *)getWindowPointer())->getWindowHandle(),
			m_renderer.getDevice(),
			m_renderer.getPhysicalDevice(),
			m_renderer.getGraphicsQueue(),
			(uint32_t)
			queueFamilyIndices.graphicsFamily,
			m_renderer.getSwapChainFrameBuffers().data(),
			(uint32_t)
			m_renderer.getSwapChainFrameBuffers()
			.size(),
			m_renderer.getSwapChainImageFormat(),
			m_renderer.getDepthMap()->m_format,
			NK_GLFW3_DEFAULT);

		// /* Load Fonts: if none of these are loaded a default font will be used  */
		// /* Load Cursor: if you uncomment cursor loading please hide the cursor */
		{
			nk_glfw3_font_stash_begin(&atlas);
			struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "media/fonts/DroidSans.ttf", 18, 0);
			/*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "fonts/Roboto-Regular.ttf", 14, 0);*/
			/*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "fonts/kenvector_future_thin.ttf", 13, 0);*/
			/*struct nk_font *clean = nk_font_atlas_add_from_file(atlas,  "fonts/ProggyClean.ttf", 12, 0);*/
			/*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas,   "fonts/ProggyTiny.ttf", 10, 0);*/
			/*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "fonts/Cousine-Regular.ttf", 13, 0);*/
			nk_glfw3_font_stash_end();
			/*nk_style_load_all_cursors(ctx, atlas->cursors);*/
			nk_style_set_font(m_ctx, &droid->handle);

			/* style.c */
			/*set_style(ctx, THEME_WHITE);*/
			/*set_style(ctx, THEME_RED);*/
			/*set_style(ctx, THEME_BLUE);*/
			/*set_style(ctx, THEME_DARK);*/
		}
	}

	void VESubrender_Nuklear::closeSubrenderer()
	{
		nk_glfw3_shutdown();
	}

	void VESubrender_Nuklear::prepareDraw()
	{
		nk_glfw3_new_frame();
	}

	VkSemaphore VESubrender_Nuklear::draw(uint32_t
		imageIndex,
		VkSemaphore wait_semaphore)
	{
		return nk_glfw3_render(NK_ANTI_ALIASING_ON, imageIndex, wait_semaphore);
	}

	void *VESubrender_Nuklear::addTexture(VETexture *texture)
	{
		return nk_glfw3_add_image(texture->m_imageInfo.sampler, texture->m_imageInfo.imageView);
	}

} // namespace ve
