//common definitions

//light defintion
//type: 0-directional, 1-point, 2-spot
//param: light parameters

#define NUM_SHADOW_CASCADE 6

#define LIGHT_DIR 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
#define LIGHT_AMBIENT 3

struct cameraData_t {
  mat4 camModel;
  mat4 camView;
  mat4 camProj;
  vec4 param;
  vec4 a,b,c;
};

struct lightData_t {
  ivec4 itype;
  mat4  lightModel;
  vec4  col_ambient;
  vec4  col_diffuse;
  vec4  col_specular;
  vec4  param;
  vec4  a,b,c,d,e,f,g;
  cameraData_t shadowCameras[NUM_SHADOW_CASCADE];
};

struct objectData_t {
  mat4 model;
  mat4 modelInvTrans;
  vec4 color;
  vec4 param;
  ivec4 iparam;
  vec4  a,b,c,d,e;
};
