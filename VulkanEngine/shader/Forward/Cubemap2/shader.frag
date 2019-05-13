#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shading_language_420pack : enable

#define RESOURCEARRAYLENGTH 16

#include "../common_defines.glsl"

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout (set = 4, binding = 0) uniform sampler2DArray samplerCubeMapArray[RESOURCEARRAYLENGTH];

layout(location = 0) in vec3 fragPosL;
layout(location = 1) in vec3 fragPosW;

layout (location = 0) out vec4 outFragColor;


void main()
{
  ivec4 iparam    = objectUBO.data.iparam;
  uint resIdx     = iparam.x % RESOURCEARRAYLENGTH;

  vec2 uv = fragPosW.yz;
  int idx=0;

  if( fragPosW.x<-249.999 ) { //
    idx = 1;
    uv = fragPosW.zy*-1;
  }
  if( fragPosW.x>249.999  ) { //
    idx = 0;
    uv = fragPosW.zy;
  }
  if( fragPosW.y<-249.999  ) {
    idx = 2;
    uv = fragPosW.xz;
  }
  if( fragPosW.y>249.999  ) { //
    idx = 3;
    uv = fragPosW.xz;
  }
  if( fragPosW.z<-249.999  ) { //
    idx = 4;
    uv = fragPosW.xy;
  }
  if( fragPosW.z>249.999  ) {
    idx = 5;
    uv = fragPosW.xy*-1;
  }
  uv = uv / 500 + 0.5;

	outFragColor = vec4(texture( samplerCubeMapArray[resIdx], vec3( uv, idx ) ).xyz, 1);
}
