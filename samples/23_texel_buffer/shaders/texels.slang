[[vk::binding(0, 0)]] Buffer<uint> source;
[[vk::binding(1, 0)]]
[[vk::image_format("r32ui")]]
RWBuffer<uint> destination;
struct Parameters { uint count; };
[[vk::push_constant]] Parameters parameters;

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x < parameters.count)
        destination[tid.x] = source[tid.x] ^ 0x5a5a5a5a;
}
