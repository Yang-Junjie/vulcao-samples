struct Uniforms
{
    float4x4 mvp;
};

[[vk::binding(0, 0)]] ConstantBuffer<Uniforms> uniforms;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float3 position : POSITION,
               [[vk::location(1)]] float3 color : COLOR)
{
    VSOut output;
    output.position = mul(uniforms.mvp, float4(position, 1.0));
    output.color = color;
    return output;
}

[shader("fragment")]
float4 fragMain([[vk::location(0)]] float3 color) : SV_Target
{
    return float4(color, 1.0);
}
