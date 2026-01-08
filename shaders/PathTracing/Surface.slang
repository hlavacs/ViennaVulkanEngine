import Frame;

public struct IntersectionRecord
{
    public float3 position;
    public float3 direction;
    public float3 normal;
    public float3 tangent;
    public float2 uv;
    public Frame frame;
    public uint materialIdx;
};