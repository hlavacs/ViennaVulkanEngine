/** @file simple_atmosphere.slang
 * @brief Physical sky, sun, weather haze and volumetric clouds (SkySettings); included by simple_forward.slang.
 *
 * World units are metres, sea level is y = 0. The sky integrates single Rayleigh and Mie scattering of the sun
 * (frame.skySun) plus an isotropic share for multiple scattering; rays curve upwards with the earth, which bounds the
 * horizon. Clouds are ray-marched through procedural value noise between frame.cloudLayer.x and x + y, lit by a short
 * march towards the sun. The classic height haze (frame.fog*) becomes weather haze lit by sun and sky. Without bindings of
 * its own, reflections and sky light use the same sky model, blended from horizon to zenith and greyed by cloud cover.
 */

static const float3 kRayleigh = float3(5.8e-6, 13.6e-6, 33.1e-6); ///< Air scattering per metre at sea level; blue scatters most.
static const float kMie = 1.2e-5;           ///< Aerosol scattering per metre of a clear day at sea level, times skyParams.x.
static const float kEarthRadius = 6.36e6;   ///< Curvature that bounds horizontal rays.
static const float kAtmosphereTop = 6.0e4;  ///< Height where the integrated atmosphere ends.

bool physicalSky() { return frame.skySun.w > 0.0; }
bool cloudsEnabled() { return physicalSky() && frame.cloudLayer.z > 0.0; }

float henyeyGreenstein(float cosine, float g)
{
    return (1.0 - g * g) / (4.0 * kPi * pow(max(1.0 + g * g - 2.0 * g * cosine, 1.0e-4), 1.5));
}

/// @brief Optical depth over d metres of a medium whose density falls by exp(-falloff * height): density at y0 times d times the mean factor.
float exponentialDepth(float falloff, float y0, float dy, float d)
{
    const float k = falloff * dy;
    const float mean = abs(k) < 0.001 ? 1.0 : (1.0 - exp(clamp(-k, -80.0, 80.0))) / k;
    return exp(clamp(-falloff * y0, -80.0, 80.0)) * mean * d;
}

/// @brief Optical depth of the height haze (AtmosphereSettings) over d units of a ray; the classic haze uses it too.
float hazeDepth(float3 eye, float3 dir, float d)
{
    return frame.fogColorDensity.w * exponentialDepth(frame.fogHeightTime.y, eye.y - frame.fogHeightTime.x, dir.y * d, d);
}

/// @brief Optical depth of air and aerosols over d metres of a straight ray.
float3 airDepth(float3 eye, float3 dir, float d)
{
    return kRayleigh * exponentialDepth(1.0 / 8000.0, max(eye.y, 0.0), dir.y * d, d) +
        (kMie * frame.skyParams.x / 0.9) * exponentialDepth(1.0 / 1200.0, max(eye.y, 0.0), dir.y * d, d);
}

/// @brief Height after t metres along a ray with vertical cosine mu, which rises above the curved ground.
float heightAlong(float h, float mu, float t) { return h + t * mu + t * t / (2.0 * kEarthRadius); }

/// @brief Distance from height h along vertical cosine mu to the top of the atmosphere.
float atmosphereLength(float h, float mu) { return kEarthRadius * (sqrt(mu * mu + 2.0 * (kAtmosphereTop - h) / kEarthRadius) - mu); }

/// @brief Optical depth (rgb) from height h towards the sun, in four samples.
[noinline] // Keeps the many call sites from multiplying the shader size.
float3 sunDepth(float h)
{
    const float mu = max(frame.skySun.y, 0.0);
    const float pathLength = atmosphereLength(h, mu);
    float3 depth = float3(0.0);
    float previous = 0.0;
    for (uint i = 1; i <= 4; ++i) {
        const float t = pathLength * float(i * i) / 16.0; // Quadratic spacing resolves the dense air near the start.
        const float y = max(heightAlong(h, mu, 0.5 * (previous + t)), 0.0);
        depth += (kRayleigh * exp(-y / 8000.0) + (kMie * frame.skyParams.x / 0.9) * exp(-y / 1200.0)) * (t - previous);
        previous = t;
    }
    return depth;
}

/// @brief Sun irradiance after its path through the air down to height h.
float3 sunlightAt(float h) { return frame.skySun.w * exp(-sunDepth(max(h, 0.0))); }

/// @brief Light a cloud of some optical depth lets through by scattering many times, per unit irradiance (two-stream slab, darkened).
float diffuseTransmission(float depth) { return 1.0 / (1.0 + 0.25 * depth); }

/// @brief Average shade of the clouds on air below their top: covered share without direct sun, and the light the deck passes.
float2 cloudCover(float y)
{
    if (!cloudsEnabled() || y > frame.cloudLayer.x + frame.cloudLayer.y) return float2(0.0, 0.0);
    const float cover = frame.cloudLayer.z * frame.cloudLayer.z;
    return float2(cover, cover * diffuseTransmission(frame.cloudLayer.w * frame.cloudLayer.y));
}

/// @brief Sky radiance towards a direction seen from height h; below the horizon it repeats the horizon, which scene geometry hides.
[noinline]
float3 skyRadiance(float h, float3 direction, uint steps)
{
    const float3 dir = normalize(float3(direction.x, max(direction.y, 0.0), direction.z) + float3(0.0, 1.0e-4, 0.0));
    const float3 sun = frame.skySun.xyz;
    const float cosine = dot(dir, sun);
    const float rayleighPhase = 3.0 / (16.0 * kPi) * (1.0 + cosine * cosine) + 0.05; // + isotropic multiple scattering
    const float miePhase = henyeyGreenstein(cosine, 0.8) + 0.05;
    const float altitude = max(h, 0.0);
    const float pathLength = atmosphereLength(altitude, dir.y);
    float3 depth = float3(0.0), light = float3(0.0), diffuse = float3(0.0);
    float previous = 0.0;
    for (uint i = 1; i <= steps; ++i) {
        const float t = pathLength * float(i * i) / float(steps * steps);
        const float dt = t - previous;
        const float y = max(heightAlong(altitude, dir.y, 0.5 * (previous + t)), 0.0);
        const float3 rayleigh = kRayleigh * exp(-y / 8000.0);
        const float mie = kMie * frame.skyParams.x * exp(-y / 1200.0);
        const float3 transmittance = exp(-(depth + sunDepth(y)));
        const float2 cover = cloudCover(y);
        light += transmittance * (rayleigh * rayleighPhase + mie * miePhase) * ((1.0 - cover.x) * dt);
        diffuse += transmittance * (rayleigh + mie) * (cover.y * dt);
        depth += (rayleigh + mie / 0.9) * dt;
        previous = t;
    }
    return frame.skySun.w * (light + diffuse * (saturate(sun.y + 0.1) / kPi));
}

/// @brief Light reaching the eye from a direction for reflections, sky light and distant haze: horizon to zenith, greyed by clouds.
[noinline]
float3 skyLight(float3 eye, float3 dir)
{
    float3 light = skyRadiance(eye.y, float3(dir.x, 0.0, dir.z), 6);
    if (dir.y > 0.01) light = lerp(light, skyRadiance(eye.y, float3(0.0, 1.0, 0.0), 6), sqrt(dir.y));
    const float2 cover = cloudCover(eye.y);
    if (cover.x <= 0.0) return light;
    const float3 overcast = frame.skySun.w * (cover.y / cover.x) * saturate(frame.skySun.y + 0.1) / kPi * 0.9;
    return lerp(light, overcast, cover.x);
}

/// @brief The sun's disk with limb darkening; zero elsewhere. Clamped so half-float targets stay finite.
float3 sunDisk(float3 dir)
{
    const float radius = frame.skyParams.y;
    const float r = sqrt(max(2.0 * (1.0 - dot(dir, frame.skySun.xyz)), 0.0)) / max(radius, 1.0e-6);
    if (radius <= 0.0 || r >= 1.0 || dir.y < 0.0) return float3(0.0);
    const float limb = 1.0 - 0.6 * (1.0 - sqrt(1.0 - r * r));
    return exp(-airDepth(float3(0.0), dir, 1.0e5)) * min(frame.skySun.w / (kPi * radius * radius), 3000.0) * limb * saturate((1.0 - r) * 6.0);
}

/// @brief Hash of a lattice point in [0, 1) (Quilez).
float latticeHash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1) * 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

/// @brief Smooth value noise in [0, 1].
float valueNoise(float3 x)
{
    const float3 i = floor(x), f = frac(x), u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(latticeHash(i), latticeHash(i + float3(1, 0, 0)), u.x), lerp(latticeHash(i + float3(0, 1, 0)), latticeHash(i + float3(1, 1, 0)), u.x), u.y),
        lerp(lerp(latticeHash(i + float3(0, 0, 1)), latticeHash(i + float3(1, 0, 1)), u.x), lerp(latticeHash(i + float3(0, 1, 1)), latticeHash(i + float3(1, 1, 1)), u.x), u.y), u.z);
}

/// @brief Cloud density from 0 to 1; detail adds small-scale erosion for view rays.
[noinline]
float cloudDensity(float3 p, bool detail)
{
    const float layer = (p.y - frame.cloudLayer.x) / frame.cloudLayer.y;
    if (layer <= 0.0 || layer >= 1.0) return 0.0;
    const float coverage = frame.cloudLayer.z;
    const float3 q = (p + float3(frame.cloudWind.x, 0.0, frame.cloudWind.y)) / 1500.0;
    // Large cloud fields, single clouds and their lumps; flat bases and rounded tops.
    float shape = 0.5 * valueNoise(q * 0.2) + 0.33 * valueNoise(q * 0.9) + 0.17 * valueNoise(q * 2.3);
    if (detail) shape -= 0.12 * valueNoise(q * 7.0) * (1.0 - layer);
    const float profile = saturate(layer * 10.0) * saturate((1.0 - layer) / (1.0 - lerp(0.3, 0.8, coverage)));
    return saturate((shape * profile - (1.0 - coverage) * 0.75) * 4.0) * coverage;
}

/// @brief Clouds between the eye and maxDistance: rgb light (faded towards skyBehind by the air in front), a transmittance.
[noinline]
float4 marchClouds(float3 eye, float3 dir, float maxDistance, float3 skyBehind, float dither)
{
    const float rise = abs(dir.y) < 1.0e-4 ? 1.0e-4 : dir.y;
    const float t0 = (frame.cloudLayer.x - eye.y) / rise, t1 = (frame.cloudLayer.x + frame.cloudLayer.y - eye.y) / rise;
    const float start = max(min(t0, t1), 0.0);
    const float end = min(min(max(t0, t1), maxDistance), start + 6.0e4);
    if (end <= start) return float4(0.0, 0.0, 0.0, 1.0);
    const uint steps = 48;
    const float dt = (end - start) / float(steps);
    const float3 sun = frame.skySun.xyz;
    const float cosine = dot(dir, sun);
    const float phase = lerp(henyeyGreenstein(cosine, 0.8), henyeyGreenstein(cosine, -0.3), 0.3);
    const float3 sunlight = sunlightAt(frame.cloudLayer.x + frame.cloudLayer.y);
    const float3 above = skyRadiance(frame.cloudLayer.x + frame.cloudLayer.y, float3(0.0, 1.0, 0.0), 6);
    float3 light = float3(0.0);
    float transmittance = 1.0, distance = 0.0, weight = 0.0;
    for (uint i = 0; i < steps && transmittance > 0.01; ++i) {
        const float t = start + (float(i) + dither) * dt;
        const float3 p = eye + dir * t;
        const float density = cloudDensity(p, true);
        if (density <= 0.0) continue;
        float lightDepth = 0.0; // Four samples towards the sun, 50 to 800 m away.
        for (uint j = 1; j <= 4; ++j) lightDepth += cloudDensity(p + sun * (50.0 * float(j * j)), false) * 50.0 * float(2 * j - 1);
        lightDepth *= frame.cloudLayer.w;
        // Direct light, then light scattered many times, which keeps cloud bases and overcast skies grey rather than black.
        const float3 scattered = sunlight * (phase * exp(-lightDepth) + 1.5 * diffuseTransmission(lightDepth) / (4.0 * kPi)) +
            above * (0.4 + 0.6 * saturate((p.y - frame.cloudLayer.x) / frame.cloudLayer.y));
        const float absorbed = transmittance * (1.0 - exp(-density * frame.cloudLayer.w * dt));
        light += absorbed * scattered;
        distance += absorbed * t;
        weight += absorbed;
        transmittance -= absorbed;
    }
    if (transmittance <= 0.01) transmittance = 0.0; // The rest of a thick cloud hides even the sun.
    const float3 air = exp(-airDepth(eye, dir, weight > 0.0 ? distance / weight : end));
    return float4(light * air + skyBehind * (1.0 - air) * (1.0 - transmittance), transmittance);
}

/// @brief Sunlight share reaching a point through the clouds above it.
[noinline]
float cloudShadow(float3 p)
{
    if (!cloudsEnabled() || p.y >= frame.cloudLayer.x + frame.cloudLayer.y) return 1.0;
    const float3 sun = frame.skySun.xyz;
    const float rise = max(sun.y, 0.05);
    const float start = max(frame.cloudLayer.x - p.y, 0.0) / rise, step = frame.cloudLayer.y / rise / 3.0;
    float depth = 0.0;
    for (uint i = 0; i < 3; ++i) depth += cloudDensity(p + sun * (start + (float(i) + 0.5) * step), false) * step;
    return exp(-depth * frame.cloudLayer.w);
}

/// @brief Weather haze light: forward-scattered sun through the clouds plus sky light.
float3 hazeLight(float3 eye, float3 dir)
{
    return 0.9 * (sunlightAt(eye.y) * cloudShadow(eye) * henyeyGreenstein(dot(dir, frame.skySun.xyz), 0.5) + skyLight(eye, float3(0.0, 1.0, 0.0)));
}

/// @brief Sky, sun, clouds and haze seen in one direction from the camera.
float3 skyColour(float3 dir, float dither)
{
    const float3 eye = worldEye();
    const float3 sky = skyRadiance(eye.y, dir, 16);
    float4 clouds = float4(0.0, 0.0, 0.0, 1.0);
    if (cloudsEnabled()) clouds = marchClouds(eye, dir, 1.0e9, skyLight(eye, dir), dither); // Air in front of the clouds lies under them.
    const float3 colour = (sky + sunDisk(dir)) * clouds.a + clouds.rgb;
    if (frame.fogColorDensity.w == 0.0) return colour;
    const float haze = exp(-hazeDepth(eye, dir, 1.0e5));
    return colour * haze + hazeLight(eye, dir) * (1.0 - haze);
}

/// @brief Air, clouds and haze between the eye and a surface; distant surfaces fade into the horizon sky.
float3 applySkyAtmosphere(float3 colour, float3 worldPos)
{
    const float3 eye = worldEye();
    const float d = length(worldPos - eye);
    if (d <= 0.0) return colour;
    const float3 dir = (worldPos - eye) / d;
    const float3 horizon = skyLight(eye, float3(dir.x, 0.0, dir.z));
    const float3 air = exp(-airDepth(eye, dir, d));
    colour = colour * air + horizon * (1.0 - air);
    if (cloudsEnabled() && max(eye.y, worldPos.y) > frame.cloudLayer.x && min(eye.y, worldPos.y) < frame.cloudLayer.x + frame.cloudLayer.y) {
        const float4 clouds = marchClouds(eye, dir, d, horizon, latticeHash(worldPos));
        colour = colour * clouds.a + clouds.rgb;
    }
    if (frame.fogColorDensity.w == 0.0) return colour;
    const float haze = exp(-hazeDepth(eye, dir, d));
    return colour * haze + hazeLight(eye, dir) * (1.0 - haze);
}
