/*
* Given a frag coordinate in homogeneous space of the camera,
* find the index of the shadow camera from a directional light
* that the frag belongs to (0-3)
*/
int shadowIdxDirectional(vec4 camParam, vec4 fragCoord, float z1, float z2, float z3) {
    int sIdx = 0;
    float z = (fragCoord.z / fragCoord.w) / (camParam[1] - camParam[0]);

    if (z >= z1) {
        sIdx = 1;
    }
    if (z >= z2) {
        sIdx = 2;
    }
    if (z >= z3) {
        sIdx = 3;
    }

    return sIdx;
}


int shadowIdxSpot(vec4 fragPosW, mat4 shadowCamView, mat4 shadowCamProj, float z1, float z2, float z3) {
    return 0;
}

/*
* Given a frag coordinate in world space,
* find the index of the shadow camera from a point light
* that the frag belongs to (0-3)
*/
int shadowIdxPoint(vec3 lightPosW, vec3 fragPosW) {
    vec3 L = normalize(fragPosW - lightPosW);
    int idx = 0;
    float m = dot(L, vec3(1.0, 0.0, 0.0));

    float n = dot(L, vec3(-1.0, 0.0, 0.0));
    if (n>m) { idx = 1; m = n; }
    n = dot(L, vec3(0.0, 1.0, 0.0));
    if (n>m) { idx = 2; m = n; }
    n = dot(L, vec3(0.0, -1.0, 0.0));
    if (n>m) { idx = 3; m = n; }
    n = dot(L, vec3(0.0, 0.0, 1.0));
    if (n>m) { idx = 4; m = n; }
    n = dot(L, vec3(0.0, 0.0, -1.0));
    if (n>m) { idx = 5; m = n; }

    return idx;
}



float shadowFactor(vec3 fragposW, mat4 shadowView, mat4 shadowProj, sampler2D shadowMap, vec2 offset) {

    vec4 fragposH = shadowProj * shadowView * vec4(fragposW, 1);
    fragposH /= fragposH.w;//homogeneous coords are in [-1,1]

    fragposH.x = fragposH.x / 2.0 + 0.5;//translate to [0,1]
    fragposH.y = fragposH.y / 2.0 + 0.5;//translate to [0,1]

    float bias = 0.0001;
    float visibility = 1.0;
    if (length(texture(shadowMap, fragposH.xy + offset).rgb)  <  fragposH.z + bias) {
        visibility = 0.2;
    }
    return visibility;
}


float shadowFunc(vec3 fragposW, mat4 shadowView, mat4 shadowProj, sampler2D shadowMap) {

    ivec2 texDim = textureSize(shadowMap, 0);
    float scale = 1.2;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float factor = 0.0;
    float sum = 0;
    int range = 1;
    float weight = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            weight = 1.0;
            factor += weight*shadowFactor(fragposW, shadowView, shadowProj, shadowMap, vec2(dx*x, dy*y));
            sum += weight;
        }
    }
    return factor / sum;
}



vec3 dirlight(int lightType, vec3 camposW,
vec3 lightdirW, vec4 lightparam, float shadowFac,
vec3 ambcol, vec3 diffcol, vec3 speccol,
vec3 fragposW, vec3 fragnormalW, vec3 fragcolor) {

    if (lightType != LIGHT_DIR) return vec3(0, 0, 0);

    vec3 viewDirW  = normalize(camposW - fragposW);

    //start light calculations
    vec3 lightVectorW = normalize(lightdirW);

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(reflect(lightdirW, fragnormalW));
    float spec = pow(max(dot(viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    //return ambcol * shadowFac;
    return (ambcol + shadowFac*(diffuse + specular)) * fragcolor;
}


vec3 pointlight(int lightType, vec3 camposW,
vec3 lightposW, vec4 lightparam, float shadowFac,
vec3 ambcol, vec3 diffcol, vec3 speccol,
vec3 fragposW, vec3 fragnormalW, vec3 fragcolor) {

    if (lightType != LIGHT_POINT) return vec3(0, 0, 0);

    vec3 viewDirW  = normalize(camposW - fragposW);

    //start light calculations
    vec3 lightVectorW = fragposW - lightposW;
    float distance = length(lightVectorW);
    float strength = clamp(1 - distance / lightparam[0], 0, 1);

    lightVectorW = normalize(lightVectorW);

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(reflect(lightVectorW, fragnormalW));
    float spec = pow(max(dot(viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    return (ambcol + strength*shadowFac*(diffuse + specular)) * fragcolor;
}


vec3 spotlight(int lightType, vec3 camposW,
vec3 lightposW, vec3 lightdirW, vec4 lightparam, float shadowFac,
vec3 ambcol, vec3 diffcol, vec3 speccol,
vec3 fragposW, vec3 fragnormalW, vec3 fragcolor) {

    if (lightType != LIGHT_SPOT) return vec3(0, 0, 0);

    vec3 viewDirW  = normalize(camposW - fragposW);

    //start light calculations
    vec3 lightVectorW = fragposW - lightposW;
    float distance = length(lightVectorW);
    float strength = clamp(1 - distance / lightparam[0], 0, 1);

    lightVectorW = normalize(lightVectorW);
    float spotFactor = pow(max(dot(lightVectorW, lightdirW), 0.0), 10.0);

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = spotFactor * diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(reflect(lightdirW, fragnormalW));
    float spec = pow(max(dot(viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spotFactor * spec * speccol;

    //add up to get the result
    return (ambcol + strength*shadowFac*(diffuse + specular)) * fragcolor;
}
