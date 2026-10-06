[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output;
[[vk::binding(1, 0)]] Texture2D<float4> textures[];
struct Parameters { uint count; };
[[vk::push_constant]] Parameters parameters;

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= parameters.count)
        return;
    // Slots 1, 3 and 5 are unbound and must never be accessed.
    const uint index = (tid.x % 3) * 2;
    output[tid.x] = uint(round(textures[NonUniformResourceIndex(index)].Load(int3(0, 0, 0)).r * 255.0));
}
