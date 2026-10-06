struct VSOut { float4 position : SV_Position; float3 color : COLOR; };

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION,
               [[vk::location(1)]] float3 color : COLOR)
{
    VSOut output;
    output.position = float4(position, 0.0, 1.0);
    output.color = color;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    return float4(input.color, 1.0);
}
