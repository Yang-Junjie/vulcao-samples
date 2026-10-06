[[vk::binding(0, 0)]] RWStructuredBuffer<float2> vertices;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> arguments;

[shader("compute")]
[numthreads(1, 1, 1)]
void compMain()
{
    vertices[0] = float2(0.0, -0.75);
    vertices[1] = float2(0.75, 0.75);
    vertices[2] = float2(-0.75, 0.75);
    // VkDrawIndirectCommand: vertexCount, instanceCount, firstVertex, firstInstance.
    arguments[0] = 3;
    arguments[1] = 1;
    arguments[2] = 0;
    arguments[3] = 0;
}
