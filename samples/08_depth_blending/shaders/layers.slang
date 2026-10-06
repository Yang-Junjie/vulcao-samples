struct ObjectData
{
    float4 color;
    float depth;
    float3 padding;
};
[[vk::push_constant]] ObjectData object;

[shader("vertex")]
float4 vertMain([[vk::location(0)]] float2 position : POSITION) : SV_Position
{
    return float4(position, object.depth, 1.0);
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return object.color;
}
