//common definitions

//light defintion
//type: 0-directional, 1-point, 2-spot
//param: light parameters

struct light_t {
  ivec4   itype;
  vec4   param;
  vec4   col_ambient;
  vec4   col_diffuse;
  vec4   col_specular;
  mat4x4 transform;
};




