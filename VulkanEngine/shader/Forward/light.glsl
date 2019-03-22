
vec3 dirlight(  vec3 campos,
                vec3 lightdir, vec4 lightparam,
                vec3 ambcol, vec3 diffcol, vec3 speccol,
                vec3 fragpos, vec3 fragnormal, vec3 fragcolor ) {

    vec3 viewDir  = normalize( campos - fragpos );

    //start light calculations
    vec3 lightVector = normalize( lightdir );

    //diffuse
    float diff = max(dot(fragnormal, -lightVector), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDir = normalize(  reflect( lightdir, fragnormal )  );
    float spec = pow( max( dot( viewDir, reflectDir), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    return (ambcol + diffuse + specular) * fragcolor;
}


vec3 pointlight(  vec3 campos,
                  vec3 lightpos, vec4 lightparam,
                  vec3 ambcol, vec3 diffcol, vec3 speccol,
                  vec3 fragpos, vec3 fragnormal, vec3 fragcolor ) {

    vec3 viewDir  = normalize( campos - fragpos );

    //start light calculations
    vec3 lightVector = normalize( fragpos - lightpos );

    //diffuse
    float diff = max(dot(fragnormal, -lightVector), 0.0);
    vec3 diffuse = diff * diffcol;

    //specular
    vec3 reflectDir = normalize(  reflect( lightVector, fragnormal )  );
    float spec = pow( max( dot( viewDir, reflectDir), 0.0), 2.0);
    vec3 specular = spec * speccol;

    //add up to get the result
    return (ambcol + diffuse + specular) * fragcolor;
}


vec3 spotlight( vec3 campos,
                vec3 lightpos, vec3 lightdir, vec4 lightparam,
                vec3 ambcol, vec3 diffcol, vec3 speccol,
                vec3 fragpos, vec3 fragnormal, vec3 fragcolor ) {

    vec3 viewDir  = normalize( campos - fragpos );

    //start light calculations
    vec3 lightVector = normalize(fragpos - lightpos);
    float spotFactor = pow( max( dot( lightVector, lightdir), 0.0), 10.0);

    //diffuse
    float diff = max(dot(fragnormal, -lightVector), 0.0);
    vec3 diffuse = spotFactor * diff * diffcol;

    //specular
    vec3 reflectDir = normalize(  reflect( lightdir, fragnormal )  );
    float spec = pow( max( dot( viewDir, reflectDir), 0.0), 2.0);
    vec3 specular = spotFactor * spec * speccol;

    //add up to get the result
    return (ambcol + diffuse + specular) * fragcolor;
}
