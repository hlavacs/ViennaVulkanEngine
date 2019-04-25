

#include "VEInclude.h"


namespace ve {

	/**
	*
	* \brief Callback for frame ended event
	*
	* \param[in] event The frame ended event
	*
	*/
	void VEEventListenerNuklearDebug::onFrameEnded(veEvent event) {
		VESubrenderFW_Nuklear * pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
		if (pSubrender == nullptr) return;

		struct nk_context * ctx = pSubrender->getContext();

		/* GUI */
		if (nk_begin(ctx, "An error occured", nk_rect(	(float)getWindowPointer()->getExtent().width / 2, (float)getWindowPointer()->getExtent().height / 2,
														(float)getWindowPointer()->getExtent().width/2, (float)getWindowPointer()->getExtent().height/2),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			nk_layout_row_dynamic(ctx, 30, 1);

			nk_label(ctx, (std::string("FPS: ") + getName()).c_str(), NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);

			if (nk_button_label(ctx, "Close Engine")) {
				getEnginePointer()->end();
			}
		}
		nk_end(ctx);

	}

}

