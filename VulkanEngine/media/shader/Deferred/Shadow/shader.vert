#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "../../common_defines.glsl"

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(location = 0) in vec3 inPositionL;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position    = cameraUBO.data.camProj * cameraUBO.data.camView * objectUBO.data.model * vec4(inPositionL, 1.0);
}