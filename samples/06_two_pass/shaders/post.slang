// Pass 2 of the 06_two_pass sample: samples the linear HDR render of pass 1,
// applies exposure, ACES tonemapping and gamma, and draws a fullscreen triangle
// into the swapchain.
//
// The triangle comes from a vertex buffer rather than SV_VertexID on purpose,
// matching 04_offscreen: SV_VertexID needs the shader draw parameters device
// feature and this sample keeps its requirements at their minimum.

[[vk::binding(0, 0)]] Sampler2D scene;

struct PostUniforms
{
    float exposure;
    float gamma; // exponent applied to the tonemapped color, i.e. 1/2.2
    float2 padding;
};

[[vk::push_constant]] PostUniforms post;

struct VSOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION)
{
    VSOut output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = position * 0.5 + 0.5;
    return output;
}

// Narkowicz's ACES filmic curve, which rolls HDR highlights off into [0, 1]
// instead of clipping them.
float3 tonemap_aces(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    const float3 hdr = scene.Sample(input.uv).rgb * post.exposure;
    const float3 mapped = tonemap_aces(hdr);
    // The swapchain is a UNORM format, so the transfer function is applied here
    // rather than by the hardware.
    return float4(pow(mapped, post.gamma), 1.0);
}
