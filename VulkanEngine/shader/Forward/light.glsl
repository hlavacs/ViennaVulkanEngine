

float shadowFunc(  vec3 fragposW, mat4 shadowView, mat4 shadowProj, sampler2D shadowMap ) {

    vec4 fragposH = shadowProj * shadowView * vec4(fragposW,1);
    fragposH /= fragposH.w;                 //homogeneous coords are in [-1,1]

    fragposH.xz = fragposW.xy / 2 + 0.5;     //translate to [0,1]

    float visibility = 1.0;
    if ( texture( shadowMap, fragposH.xy ).r  <  fragposH.z){
        visibility = 0.0;
    }
    return visibility;
}


vec3 dirlight(  vec3 camposW,
                vec3 lightdirW, vec4 lightparam, float shadowFac,
                vec3 ambcol, vec3 diffcol, vec3 speccol,
                vec3 fragposW, vec3 fragnormalW, vec3 fragcolor ) {

    vec3 viewDirW  = normalize( camposW - fragposW );

    //start light calculations
    vec3 lightVectorW = normalize( lightdirW );

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(  reflect( lightdirW, fragnormalW )  );
    float spec = pow( max( dot( viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    return (ambcol + shadowFac*(diffuse + specular)) * fragcolor;
}


vec3 pointlight(  vec3 camposW,
                  vec3 lightposW, vec4 lightparam, float shadowFac,
                  vec3 ambcol, vec3 diffcol, vec3 speccol,
                  vec3 fragposW, vec3 fragnormalW, vec3 fragcolor ) {

    vec3 viewDirW  = normalize( camposW - fragposW );

    //start light calculations
    vec3 lightVectorW = normalize( fragposW - lightposW );

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(  reflect( lightVectorW, fragnormalW )  );
    float spec = pow( max( dot( viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    return (ambcol + shadowFac*(diffuse + specular)) * fragcolor;
}


vec3 spotlight( vec3 camposW,
                vec3 lightposW, vec3 lightdirW, vec4 lightparam, float shadowFac,
                vec3 ambcol, vec3 diffcol, vec3 speccol,
                vec3 fragposW, vec3 fragnormalW, vec3 fragcolor ) {

    vec3 viewDirW  = normalize( camposW - fragposW );

    //start light calculations
    vec3 lightVectorW = normalize(fragposW - lightposW);
    float spotFactor = pow( max( dot( lightVectorW, lightdirW), 0.0), 10.0);

    //diffuse
    float diff = max(dot(fragnormalW, -lightVectorW), 0.0);
    vec3 diffuse = spotFactor * diff * diffcol;

    //specular
    vec3 reflectDirW = normalize(  reflect( lightdirW, fragnormalW )  );
    float spec = pow( max( dot( viewDirW, reflectDirW), 0.0), 2.0);
    vec3 specular = spotFactor * spec * speccol;

    //add up to get the result
    return (ambcol + shadowFac*(diffuse + specular)) * fragcolor;
}
