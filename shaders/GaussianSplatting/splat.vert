#version 460

layout(std430, set = 1, binding = 1) readonly buffer Instances {
  vec4 instances[];  // (N, 12). 3 for ndc position, 1 padding, 4 for rot scale, 4 for color.
};

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_position;

void main() {
  // index [0,1,2,2,1,3], 4 vertices for a splat.
  int index = gl_VertexIndex / 4;
  vec3 ndc_position = instances[index * 3 + 0].xyz;
  mat2 rot_scale = mat2(instances[index * 3 + 1].xy, instances[index * 3 + 1].zw);
  vec4 color = instances[index * 3 + 2];

  // quad positions (-1, -1), (-1, 1), (1, -1), (1, 1), ccw in screen space.
  int vert_index = gl_VertexIndex % 4;
  vec2 position = vec2(vert_index / 2, vert_index % 2) * 2.f - 1.f;

  // confidence_radius controls how far the gaussian extends
  // For proper falloff, position should reach ±3 at edges (exp(-0.5*9) ≈ 0.01)
  float confidence_radius = 3.0f;

  gl_Position = vec4(ndc_position + vec3(rot_scale * position * confidence_radius, 0.f), 1.f);
  out_color = color;
  // Pass scaled position to fragment shader for gaussian falloff calculation
  out_position = position * confidence_radius;
}
