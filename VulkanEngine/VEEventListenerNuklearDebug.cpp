

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
	void VEEventListenerNuklearDebug::onDrawOverlay(veEvent event)
	{
		VESubrender_Nuklear *pSubrender = (VESubrender_Nuklear *)getEnginePointer()->getRenderer()->getOverlay();
		if (pSubrender == nullptr)
			return;

		struct nk_context *ctx = pSubrender->getContext();

		/* GUI */
		if (nk_begin(ctx, "Statistics", nk_rect(0, 0, 600, 600),
			NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE |
			NK_WINDOW_TITLE))
		{
			char outbuffer[100];
			nk_layout_row_dynamic(ctx, 30, 1);
			nk_label(ctx, "LOOP", NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, " Max Frametime (ms): %4.1f", getEnginePointer()->getMaxFrameTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, " 30 FPS percentage: %4.1f", getEnginePointer()->getAcceptableLevel1() * 100.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, " 60 FPS percentage: %4.1f", getEnginePointer()->getAcceptableLevel2() * 100.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, " Avg Frametime (ms): %4.1f", getEnginePointer()->getAvgFrameTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Started (ms): %4.1f", getEnginePointer()->getAvgStartedTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Events(ms): %4.1f", getEnginePointer()->getAvgEventTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Updates (ms): %4.1f", getEnginePointer()->getAvgUpdateTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Draw (ms): %4.1f", getEnginePointer()->getAvgDrawTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Prep Ovl (ms): %4.1f", getEnginePointer()->getAvgPrepOvlTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Ended (ms): %4.1f", getEnginePointer()->getAvgEndedTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Draw Ovl (ms): %4.1f", getEnginePointer()->getAvgDrawOvlTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Present (ms): %4.1f", getEnginePointer()->getAvgPresentTime() * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			//----------------------------------------------------------
			nk_layout_row_dynamic(ctx, 30, 1);
			nk_label(ctx, "RECORDING", NK_TEXT_LEFT);

			/*
				nk_layout_row_dynamic(ctx, 30, 1);
				sprintf(outbuffer, "  Record shadow (ms): %4.1f", getEnginePointer()->getRenderer()->m_AvgCmdShadowTime * 1000.0f);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 30, 1);
				sprintf(outbuffer, "  Record light (ms): %4.1f", getEnginePointer()->getRenderer()->m_AvgCmdLightTime * 1000.0f);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 30, 1);
				sprintf(outbuffer, "  G-Buffer (ms): %4.1f", getEnginePointer()->getRenderer()->m_AvgCmdGBufferTime * 1000.0f);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);
				*/

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Record 1 buffer offscreen (ms): %4.1f",
				getEnginePointer()->getRenderer()->m_AvgRecordTimeOffscreen * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 30, 1);
			sprintf(outbuffer, "  Record 1 buffer onscreen (ms): %4.1f",
				getEnginePointer()->getRenderer()->m_AvgRecordTimeOnscreen * 1000.0f);
			nk_label(ctx, outbuffer, NK_TEXT_LEFT);
		}
		nk_end(ctx);
	}

} // namespace ve
