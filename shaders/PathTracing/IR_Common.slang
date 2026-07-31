import raytracing;

bool isPointOccluded(in float3 startPos, in float3 endPos, in float3 normal) {

    float3 toTarget = endPos - startPos;
    float distance = length(toTarget);


    // 1. Use a robust offset to prevent self-intersection
    RayDesc shadowRay;
    // shadowRay.Origin = its.position + (its.normal * 0.001);
    shadowRay.Origin = offset_ray(startPos, normal);
    shadowRay.Direction = normalize(toTarget);
    shadowRay.TMin = 0.0;
    // 2. Stop the ray just BEFORE it reaches the target point
    shadowRay.TMax = distance - 0.001;

    // 3. Horizon check (using a small epsilon to allow grazing lights)
    if (dot(shadowRay.Direction, normal) < 1e-4f) {
        return true;
    }

    HitPayload payload;
    payload.hit = false; // Ensure this is initialized!

    // 4. Optimization: Use flags to stop at the very first hit
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    // If the ray hit anything before reaching the target, it's occluded
    return payload.hit;
}