[[vk::constant_id(0)]] const uint multiplier = 2;
[[vk::constant_id(1)]] const uint addend = 1;
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output;
struct Parameters { uint count; };
[[vk::push_constant]] Parameters parameters;

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x < parameters.count)
        output[tid.x] = tid.x * multiplier + addend;
}
