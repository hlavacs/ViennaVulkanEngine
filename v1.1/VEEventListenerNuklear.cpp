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
		* \brief Callback for frame ended event
		*
		* \param[in] event The frame ended event
		*
		*/
	void VEEventListenerNuklear::onDrawOverlay(veEvent event)
	{
		VESubrender_Nuklear *pSubrender = (VESubrender_Nuklear *)getEnginePointer()->getRenderer()->getOverlay();
		if (pSubrender == nullptr)
			return;

		struct nk_context *ctx = pSubrender->getContext();

		/* GUI */
		if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
			NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			nk_layout_row_static(ctx, 30, 80, 1);
			if (nk_button_label(ctx, "button"))
				fprintf(stdout, "button pressed\n");

			nk_layout_row_dynamic(ctx, 30, 2);
			if (nk_option_label(ctx, "up", m_orientation == UP))
				m_orientation = UP;
			if (nk_option_label(ctx, "down", m_orientation == DOWN))
				m_orientation = DOWN;

			nk_layout_row_dynamic(ctx, 25, 1);
			nk_property_int(ctx, "Zoom:", 0, &m_zoom, 100, 10, 1);

			nk_layout_row_dynamic(ctx, 20, 1);
			nk_label(ctx, "background:", NK_TEXT_LEFT);
			nk_layout_row_dynamic(ctx, 25, 1);
			if (nk_combo_begin_color(ctx, m_background, nk_vec2(nk_widget_width(ctx), 400)))
			{
				nk_layout_row_dynamic(ctx, 120, 1);
				m_background = nk_color_picker(ctx, m_background, NK_RGBA);
				nk_layout_row_dynamic(ctx, 25, 1);
				m_background.r = (nk_byte)nk_propertyi(ctx, "#R:", 0, m_background.r, 255, 1, 1);
				m_background.g = (nk_byte)nk_propertyi(ctx, "#G:", 0, m_background.g, 255, 1, 1);
				m_background.b = (nk_byte)nk_propertyi(ctx, "#B:", 0, m_background.b, 255, 1, 1);
				m_background.a = (nk_byte)nk_propertyi(ctx, "#A:", 0, m_background.a, 255, 1, 1);
				nk_combo_end(ctx);
			}
		}
		nk_end(ctx);

		/* -------------- EXAMPLES ---------------- */
		/*calculator(ctx);*/
		/*overview(ctx);*/
		/*node_editor(ctx);*/
		/* ----------------------------------------- */

		/* Draw */
		// {float bg[4];
		// nk_color_fv(bg, background);
		// glfwGetWindowSize(win, &width, &height);
		// glViewport(0, 0, width, height);
		// glClear(GL_COLOR_BUFFER_BIT);
		// glClearColor(bg[0], bg[1], bg[2], bg[3]);
		// /* IMPORTANT: `nk_glfw_render` modifies some global OpenGL state
		//  * with blending, scissor, face culling, depth test and viewport and
		//  * defaults everything back into a default state.
		//  * Make sure to either a.) save and restore or b.) reset your own state after
		//  * rendering the UI. */
		// nk_glfw3_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
		// glfwSwapBuffers(win);}
		// }

		// glfwTerminate();
		// return 0;

		nk_color_fv(m_bg_color, m_background);
	}

} // namespace ve
