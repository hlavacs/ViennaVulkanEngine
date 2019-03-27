#pragma once
#include"VERenderer.h"

namespace ve {
	class VERendererPathTracing : VERenderer {
	protected:
		VkQueue m_computeQueue;

		virtual void initRenderer();
	};
}