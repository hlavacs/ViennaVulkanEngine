#pragma once

const int MAX_FRAMES_IN_FLIGHT = 2;
const uint32_t SHADOW_MAP_DIM = 4096;
const uint32_t NUM_SHADOW_CASCADE = 6;

#include "VERendererDeferred.h"
#include "VERendererForward.h"

#define DEFERRED_SHADER

#ifdef FORWARD_SHADER
#define getRendererShaderPointer() getRendererForwardPointer()
#endif
#ifdef DEFERRED_SHADER
#define getRendererShaderPointer() getRendererDeferredPointer()
#endif
#ifndef getRendererShaderPointer
#define FORWARD_SHADER
#define getRendererShaderPointer() getRendererForwardPointer()
#endif