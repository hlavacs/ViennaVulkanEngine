/** @file @brief Unlit test geometry, independent of renderer materials and output shaders. */
struct Constants {
	float4 points[3]; ///< Explicit drawable-pixel triangle coordinates.
	float2 offset; ///< Subpixel phase.
	uint detail; ///< Alpha-cutout switch.
	uint unused; ///< Push-constant alignment padding.
};
[[vk::push_constant]] Constants fixture;
/// @brief Map explicit drawable-pixel inputs to the fixed 128-square viewport.
[shader("vertex")]
float4 fixtureVertex(uint id : SV_VulkanVertexID) : SV_Position {
	// Constant loads avoid RADV's divergent push-array access defect; each draw still uses vertices 0, 1, 2.
	const float2 point = id == 0 ? fixture.points[0].xy : id == 1 ? fixture.points[1].xy : fixture.points[2].xy;
	return float4((point + fixture.offset) / 64 - 1, .5, 1);
}
/// @brief Binary shader detail deliberately remains pixel-frequency under MSAA.
void coverage(float2 p) {
	p -= fixture.offset;
	if (fixture.detail != 0 && fmod(p.y - .3125 * p.x + 128, 12) > 6) { discard; }
}
/// @brief Emit unlit white for independent linear coverage measurements.
[shader("fragment")]
float4 fixtureForward(float4 p : SV_Position) : SV_Target0 {
	coverage(p.xy);
	return float4(1);
}
/// @brief Match the public native G-buffer attachment contract.
struct GBuffer {
	float4 base : SV_Target0; ///< White base colour and roughness.
	float4 normal : SV_Target1; ///< Fixed world normal and metalness.
	float4 emission : SV_Target2; ///< Unlit flag independent of scene illumination.
};
/// @brief Preserve identical coverage in deferred and hybrid native draws.
[shader("fragment")]
GBuffer fixtureGbuffer(float4 p : SV_Position) {
	coverage(p.xy);
	GBuffer g;
	g.base = float4(1, 1, 1, .5);
	g.normal = float4(0, 0, 1, 0);
	g.emission = float4(0, 0, 0, 3);
	return g;
}
