import raytracing;

bool isPointOccluded(in float3 startPos, in float3 endPos, in float3 normal) {
    RayDesc shadowRay;
    // Offset origin slightly along the normal to prevent self-intersection
    shadowRay.Origin = startPos + (normal * 0.001);

    float3 toTarget = endPos - startPos;
    float distance = length(toTarget);

    shadowRay.Direction = toTarget / distance;
    shadowRay.TMin = 0.0;
    shadowRay.TMax = distance - 0.001; // Stop just before target

    // Horizon check
    if (dot(shadowRay.Direction, normal) <= 0.0) return true;

    HitPayload payload;
    payload.hit = false;
    const uint rayFlags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, shadowRay, payload);

    return payload.hit;
}