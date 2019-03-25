//common definitions

//light defintion
//type: 0-directional, 1-point, 2-spot
//param: light parameters

struct lightData_t {
  ivec4   itype;
  vec4   param;
  vec4   col_ambient;
  vec4   col_diffuse;
  vec4   col_specular;
  mat4x4 transform;
};

struct shadowData_t {
  mat4 shadowView;
  mat4 shadowProj;
};

struct cameraData_t {
  mat4 camModel;
  mat4 camView;
  mat4 camProj;
};

struct perFrameData_t {
  cameraData_t camera;
  lightData_t light;
  shadowData_t shadow;
};

struct perObjectData_t {
  mat4 model;
  mat4 modelInvTrans;
  vec4 color;
};