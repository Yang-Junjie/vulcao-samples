[[vk::binding(0, 0)]] RWStructuredBuffer<uint> values;
[[vk::binding(1, 0)]] Texture2D<float4> uploaded;
struct Parameters { uint count; };
[[vk::push_constant]] Parameters parameters;

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x < parameters.count)
        values[tid.x] = values[tid.x] * 3 + uint(round(uploaded.Load(int3(0, 0, 0)).r * 255.0));
}
