import Common;

struct GS_OUTPUT
{
    float4 pos : SV_POSITION;
};


[shader("fragment")]
float4 fragmentMain(GS_OUTPUT input) : SV_DEPTH
{
    return input.pos.z;
}







