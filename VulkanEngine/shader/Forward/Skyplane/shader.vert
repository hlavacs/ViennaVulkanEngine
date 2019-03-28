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

layout(location = 0) in vec3 inPositionL;
layout(location = 1) in vec3 inNormalL;
layout(location = 2) in vec3 inTangentL;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec4 glp = perFrameUBO.data.camera.camProj  * perFrameUBO.data.camera.camView * perObjectUBO.data.model * vec4(inPositionL, 1.0);
    gl_Position = vec4( glp.x, glp.y, glp.z, glp.z*1.000001 );
    fragTexCoord   = inTexCoord;
}
