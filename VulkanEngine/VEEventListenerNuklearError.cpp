

#include "VEInclude.h"


namespace ve {

	/**
	*
	* \brief Callback for frame ended event
	*
	* \param[in] event The frame ended event
	*
	*/
	void VEEventListenerNuklearError::onFrameEnded(veEvent event) {
		VESubrenderFW_Nuklear * pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
		if (pSubrender == nullptr) return;

		struct nk_context * ctx = pSubrender->getContext();

		/* GUI */
		if (nk_begin(ctx, "An error occured", nk_rect(0, 0, (float)getWindowPointer()->getExtent().width, (float)getWindowPointer()->getExtent().height),
			NK_WINDOW_BORDER | NK_WINDOW_TITLE))
		{
			nk_layout_row_dynamic(ctx, 30, 1);

			nk_label(ctx, (std::string("Error message: ") + getName()).c_str(), NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);

			if (nk_button_label(ctx, "Close Engine")) {
				getEnginePointer()->end();
			}
		}
		nk_end(ctx);

	}

}

