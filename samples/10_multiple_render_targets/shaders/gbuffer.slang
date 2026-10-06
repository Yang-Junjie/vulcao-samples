[shader("vertex")]
float4 vertMain([[vk::location(0)]] float2 position : POSITION) : SV_Position
{
    return float4(position, 0.0, 1.0);
}

struct GBuffer
{
    float4 albedo : SV_Target0;
    float4 normal_roughness : SV_Target1;
    uint object_id : SV_Target2;
};

[shader("fragment")]
GBuffer fragMain()
{
    GBuffer output;
    output.albedo = float4(0.25, 0.5, 0.75, 1.0);
    output.normal_roughness = float4(0.5, 0.5, 1.0, 0.2);
    output.object_id = 42;
    return output;
}
