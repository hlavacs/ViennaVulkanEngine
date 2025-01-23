struct CoarseVertex {
    float3 normal;
    float3 fragColor;
};

[shader("fragment")]
float4 fragmentMain(CoarseVertex coarseVertex : CoarseVertex) : SV_Target 
{
    return float4( coarseVertex.fragColor, 1.0 );
}


