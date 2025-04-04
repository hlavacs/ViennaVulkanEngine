struct PSOutput {
    float4 color : SV_Target;
};

[shader("fragment")]
PSOutput fragmentMain() {
    PSOutput output;
    output.color = float4(1.0, 0.0, 0.0, 1.0); // red
    return output;
}