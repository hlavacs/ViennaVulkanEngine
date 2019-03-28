#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObjectPerFrame {
    perFrameData_t data;
} perFrameUBO;

layout(set = 3, binding = 0) uniform sampler2D texSampler;

void main() {

    vec3 fragColor = texture(texSampler, fragTexCoord).xyz;

    outColor = vec4( fragColor, 1.0 );
}
