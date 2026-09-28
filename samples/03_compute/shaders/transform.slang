// Element-wise transform dispatched by the 03_compute sample.
//
// The sample uploads a storage buffer, dispatches one workgroup over it, copies
// the result back and verifies it on the CPU, so there is nothing to look at and
// no window is needed.

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> data;

[shader("compute")]
[numthreads(64, 1, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    data[tid.x] = data[tid.x] * 2u + 1u;
}
