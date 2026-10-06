[[vk::binding(0, 0)]] StructuredBuffer<uint> source;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> destination;
struct Parameters { uint count; };
[[vk::push_constant]] Parameters parameters;
groupshared uint partial[64];

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 group : SV_GroupID, uint local : SV_GroupIndex)
{
    const uint index = group.x * 64 + local;
    partial[local] = index < parameters.count ? source[index] : 0;
    // All threads participate, including those outside the input range.
    GroupMemoryBarrierWithGroupSync();
    for (uint stride = 32; stride > 0; stride >>= 1)
    {
        if (local < stride)
            partial[local] += partial[local + stride];
        GroupMemoryBarrierWithGroupSync();
    }
    if (local == 0)
        destination[group.x] = partial[0];
}
