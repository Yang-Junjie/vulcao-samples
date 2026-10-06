[[vk::binding(0, 0)]] RWStructuredBuffer<float4> output;
// A second descriptor set holds the reusable environment texture and sampler.
[[vk::binding(0, 1)]] TextureCube<float4> environment;
[[vk::binding(1, 1)]] SamplerState nearest_sampler;

static const float3 directions[6] = {
    float3(1, 0, 0), float3(-1, 0, 0),
    float3(0, 1, 0), float3(0, -1, 0),
    float3(0, 0, 1), float3(0, 0, -1)
};

[shader("compute")]
[numthreads(8, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x < 6)
        output[tid.x] = environment.SampleLevel(nearest_sampler, directions[tid.x], 0.0);
}
