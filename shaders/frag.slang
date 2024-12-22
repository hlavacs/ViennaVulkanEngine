struct CoarseVertex
{
    float3 fragColor;
    float2 uv;
};


[[vk::binding(1)]]
Sampler2D texSampler;


[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return texSampler.Sample(coarseVertex.uv);
}







