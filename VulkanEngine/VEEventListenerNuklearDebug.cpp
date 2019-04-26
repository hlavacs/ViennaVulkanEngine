

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
		if (nk_begin(ctx, "Statistics", nk_rect( 0, 0, 200, 200 ),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			char outbuffer[100];

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "Frametime (ms): %4.1f", getEnginePointer()->getAvgFrameTime()*1000.0f );
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "Updatetime (ms): %4.1f", getEnginePointer()->getAvgUpdateTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "Shadowtime (ms): %4.1f", getRendererForwardPointer()->m_AvgCmdShadowTime*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "Ligthtime (ms): %4.1f", getRendererForwardPointer()->m_AvgCmdLightTime*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

		}
		nk_end(ctx);

	}

}

