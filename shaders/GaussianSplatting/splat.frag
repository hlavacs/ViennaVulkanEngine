// ============================================================================
// SPLAT FRAGMENT SHADER
// Reference: third_party/vkgs/shaders/splat.frag
// Adapted from VKGS (https://github.com/jaesung-cs/vkgs)
//
// Adaptation: Extracted from VKGS standalone viewer and integrated into VVE
// - Identical to VKGS implementation
// ============================================================================
#version 460

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 position;

layout(location = 0) out vec4 out_color;

void main() {
  float gaussian_alpha = exp(-0.5f * dot(position, position));
  float alpha = color.a * gaussian_alpha;
  out_color = vec4(color.rgb, alpha);
}
