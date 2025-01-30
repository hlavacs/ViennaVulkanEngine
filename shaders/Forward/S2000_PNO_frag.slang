import Common;

struct CoarseVertex {
    float3 normal;
    float4 fragColor;
};

[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return coarseVertex.fragColor;
}


