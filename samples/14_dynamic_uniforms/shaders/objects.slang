struct ObjectData
{
    float4 color;
    float4 transform; // xy translation, zw scale
};
[[vk::binding(0, 0)]] ConstantBuffer<ObjectData> object;

[shader("vertex")]
float4 vertMain([[vk::location(0)]] float2 position : POSITION) : SV_Position
{
    return float4(position * object.transform.zw + object.transform.xy, 0.0, 1.0);
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return object.color;
}
