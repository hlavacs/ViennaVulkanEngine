#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 1, binding = 0) uniform UniformBufferObjectPerObject {
    perObjectData_t data;
} perObjectUBO;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
    gl_Position   = perFrameUBO.data.camera.camProj * perFrameUBO.data.camera.camView * perObjectUBO.data.model * vec4(inPosition, 1.0);
    fragColor     = perObjectUBO.data.color;
}
