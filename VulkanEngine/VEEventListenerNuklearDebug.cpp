

#include "VEInclude.h"


namespace ve {

	/**
	*
	* \brief Callback for frame ended event
	*
	* \param[in] event The frame ended event
	*
	*/
	void VEEventListenerNuklearDebug::onDrawOverlay(veEvent event) {
		VESubrenderFW_Nuklear * pSubrender = (VESubrenderFW_Nuklear*)getEnginePointer()->getRenderer()->getOverlay();
		if (pSubrender == nullptr) return;

		struct nk_context * ctx = pSubrender->getContext();

		/* GUI */
		if (nk_begin(ctx, "Statistics", nk_rect( 0, 0, 200, 500 ),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			char outbuffer[100];
			nk_layout_row_dynamic(ctx, 30, 1);
			nk_label(ctx, "LOOP", NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Frametime (ms): %4.1f", getEnginePointer()->getAvgFrameTime()*1000.0f );
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Started (ms): %4.1f", getEnginePointer()->getAvgStartedTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Events(ms): %4.1f", getEnginePointer()->getAvgEventTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Updates (ms): %4.1f", getEnginePointer()->getAvgUpdateTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Draw (ms): %4.1f", getEnginePointer()->getAvgDrawTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Prep Ovl (ms): %4.1f", getEnginePointer()->getAvgPrepOvlTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Ended (ms): %4.1f", getEnginePointer()->getAvgEndedTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Draw Ovl (ms): %4.1f", getEnginePointer()->getAvgDrawOvlTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Present (ms): %4.1f", getEnginePointer()->getAvgPresentTime()*1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			//----------------------------------------------------------
			nk_layout_row_dynamic(ctx, 30, 1);
			nk_label(ctx, "RECORDING", NK_TEXT_LEFT);

		}
		nk_end(ctx);

	}

}

