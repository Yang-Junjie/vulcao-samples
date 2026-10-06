[[vk::binding(0, 0)]] Sampler2D source;

struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION)
{
    VSOut output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = position * 0.5 + 0.5;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    return source.Sample(input.uv);
}
