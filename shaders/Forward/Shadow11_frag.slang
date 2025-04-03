import Common;

[shader("fragment")]
float4 fragmentMain(float4 position: SV_POSITION) : SV_DEPTH
{
    return position.z;
}







