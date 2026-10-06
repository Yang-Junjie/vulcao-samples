struct Parameters { float depth; };
[[vk::push_constant]] Parameters parameters;

[shader("vertex")]
float4 vertMain([[vk::location(0)]] float2 position : POSITION) : SV_Position
{
    return float4(position, parameters.depth, 1.0);
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
