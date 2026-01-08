public float3 SRGBToLinear(float3 c)
{
    return lerp(
        c / 12.92,
        pow((c + 0.055) / 1.055, 2.4),
        step(0.04045, c)
    );
}

public float3 LinearToSRGB(float3 c)
{
    return lerp(
        c * 12.92,
        1.055 * pow(c, 1.0 / 2.4) - 0.055,
        step(0.0031308, c)
    );
}