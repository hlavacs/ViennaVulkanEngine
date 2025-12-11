#version 460

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 position;

layout(location = 0) out vec4 out_color;

void main() {
  float gaussian_alpha = exp(-0.5f * dot(position, position));
  float alpha = color.a * gaussian_alpha;
  out_color = vec4(color.rgb, alpha);
}
