struct FrameData
{
    float time;
    float aspect;
};
[[vk::push_constant]] FrameData frame;

struct VSOut
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION,
               [[vk::location(1)]] float2 offset : TEXCOORD0,
               [[vk::location(2)]] float3 color : COLOR)
{
    const float angle = frame.time + offset.x * 2.0;
    const float c = cos(angle), s = sin(angle);
    const float2 rotated = float2(c * position.x - s * position.y,
                                  s * position.x + c * position.y);
    VSOut output;
    output.position = float4((offset + rotated) * float2(1.0 / frame.aspect, 1.0), 0.0, 1.0);
    output.color = color;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    return float4(input.color, 1.0);
}
