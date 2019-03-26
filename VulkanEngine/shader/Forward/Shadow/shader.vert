#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 1, binding = 0) uniform UniformBufferObjectPerObject {
    perObjectData_t data;
} perObjectUBO;

layout( push_constant) uniform PushBlock {
	int sIdx;
} push_block;

layout(location = 0) in vec3 inPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    shadowData_t s = perFrameUBO.data.shadow[push_block.sIdx];
    gl_Position = s.shadowProj * s.shadowView * perObjectUBO.data.model * vec4(inPosition, 1.0);
}
