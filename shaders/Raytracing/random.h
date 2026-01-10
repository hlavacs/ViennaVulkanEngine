// Shared helper functions and data for random sampling and random numbers.
vk_layout(binding = BIND_TEXTURE_NOISE) uniform sampler2D NoiseTexture;

// This "blue noise in disk" array is blue noise in a circle and is used for sampling the
// sun disk for the blue noise.
// these were generated using a modified mitchell's best candidate algorithm.
// 1) It was not calculated on a torus (no wrap around distance for points)
// 2) Candidates were forced to be in the unit circle (through rejection sampling)
// https://blog.demofox.org/2020/05/16/using-blue-noise-for-raytraced-soft-shadows/
const vec2 BlueNoiseInDisk[64] = vec2[64](
	vec2(0.478712,0.875764),
	vec2(-0.337956,-0.793959),
	vec2(-0.955259,-0.028164),
	vec2(0.864527,0.325689),
	vec2(0.209342,-0.395657),
	vec2(-0.106779,0.672585),
	vec2(0.156213,0.235113),
	vec2(-0.413644,-0.082856),
	vec2(-0.415667,0.323909),
	vec2(0.141896,-0.939980),
	vec2(0.954932,-0.182516),
	vec2(-0.766184,0.410799),
	vec2(-0.434912,-0.458845),
	vec2(0.415242,-0.078724),
	vec2(0.728335,-0.491777),
	vec2(-0.058086,-0.066401),
	vec2(0.202990,0.686837),
	vec2(-0.808362,-0.556402),
	vec2(0.507386,-0.640839),
	vec2(-0.723494,-0.229240),
	vec2(0.489740,0.317826),
	vec2(-0.622663,0.765301),
	vec2(-0.010640,0.929347),
	vec2(0.663146,0.647618),
	vec2(-0.096674,-0.413835),
	vec2(0.525945,-0.321063),
	vec2(-0.122533,0.366019),
	vec2(0.195235,-0.687983),
	vec2(-0.563203,0.098748),
	vec2(0.418563,0.561335),
	vec2(-0.378595,0.800367),
	vec2(0.826922,0.001024),
	vec2(-0.085372,-0.766651),
	vec2(-0.921920,0.183673),
	vec2(-0.590008,-0.721799),
	vec2(0.167751,-0.164393),
	vec2(0.032961,-0.562530),
	vec2(0.632900,-0.107059),
	vec2(-0.464080,0.569669),
	vec2(-0.173676,-0.958758),
	vec2(-0.242648,-0.234303),
	vec2(-0.275362,0.157163),
	vec2(0.382295,-0.795131),
	vec2(0.562955,0.115562),
	vec2(0.190586,0.470121),
	vec2(0.770764,-0.297576),
	vec2(0.237281,0.931050),
	vec2(-0.666642,-0.455871),
	vec2(-0.905649,-0.298379),
	vec2(0.339520,0.157829),
	vec2(0.701438,-0.704100),
	vec2(-0.062758,0.160346),
	vec2(-0.220674,0.957141),
	vec2(0.642692,0.432706),
	vec2(-0.773390,-0.015272),
	vec2(-0.671467,0.246880),
	vec2(0.158051,0.062859),
	vec2(0.806009,0.527232),
	vec2(-0.057620,-0.247071),
	vec2(0.333436,-0.516710),
	vec2(-0.550658,-0.315773),
	vec2(-0.652078,0.589846),
	vec2(0.008818,0.530556),
	vec2(-0.210004,0.519896) 
);

// Random direction helper functions.
// https://www.shadertoy.com/view/4tl3z4
float hash1(inout float seed) {
    return fract(sin(seed += 0.1)*43758.5453123);
}

vec2 hash2(inout float seed) {
    return fract(sin(vec2(seed+=0.1,seed+=0.1))*vec2(43758.5453123,22578.1459123));
}

vec3 hash3(inout float seed) {
    return fract(sin(vec3(seed+=0.1,seed+=0.1,seed+=0.1))*vec3(43758.5453123,22578.1459123,19642.3490423));
}

vec3 cosWeightedRandomHemisphereDirection( const vec3 n, inout float seed ) {
  	vec2 r = hash2(seed);
    
	vec3  uu = normalize( cross( n, vec3(0.0,1.0,1.0) ) );
	vec3  vv = cross( uu, n );
	
	float ra = sqrt(r.y);
	float rx = ra*cos(6.2831*r.x); 
	float ry = ra*sin(6.2831*r.x);
	float rz = sqrt( 1.0-r.y );
	vec3  rr = vec3( rx*uu + ry*vv + rz*n );
    
    return normalize( rr );
}

vec3 randomSphereDirection(inout float seed) {
    vec2 h = hash2(seed) * vec2(2.,6.28318530718)-vec2(1,0);
    float phi = h.y;
	return vec3(sqrt(1.-h.x*h.x)*vec2(sin(phi),cos(phi)),h.x);
}

vec3 randomHemisphereDirection( const vec3 n, inout float seed ) {
	vec3 dr = randomSphereDirection(seed);
	return dot(dr,n) * dr;
}


// Helper function to generate a blue noise value for each screen pixel.
vec4 GetRNGValues(vec4 fragCoord) {
	ivec2 noiseSize = textureSize(NoiseTexture, 0);
	int sampleFrame = int(FrameCount) % 256;
	float blueNoise = textureLod(NoiseTexture, vec2(fragCoord.xy) / vec2(noiseSize), 0.0).r;
	float blueNoiseRaw = blueNoise;
    blueNoise = fract(blueNoise + GOLDENRATIO_CONJUGATE * float(sampleFrame));
    float theta = blueNoise * 2.0f * M_PI;
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);
    
	return vec4(cosTheta, sinTheta, blueNoise, blueNoiseRaw);
}


// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	for (uint n = 0; n < 16; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float rand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

float uintToFloat(uint x) {
 	return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.f;
}
 
uint xorshift(inout uint rngState)
{
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return rngState;
}

float rand2(inout uint rngState) {
 	return uintToFloat(xorshift(rngState));
}
