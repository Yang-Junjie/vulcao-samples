[[vk::binding(0, 0)]]
[[vk::image_format("rgba8")]]
RWTexture2D<float4> destination;

struct SizeData { uint width; uint height; };
[[vk::push_constant]] SizeData size;

[shader("compute")]
[numthreads(8, 8, 1)]
void compMain(uint3 tid : SV_DispatchThreadID)
{
    // Both dimensions deliberately have incomplete workgroups.
    if (tid.x >= size.width || tid.y >= size.height)
        return;
    const uint checker = ((tid.x / 7) + (tid.y / 5)) & 1;
    destination[tid.xy] = float4(float(tid.x) / (size.width - 1),
                                 float(tid.y) / (size.height - 1), float(checker), 1.0);
}
