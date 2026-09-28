// Draws a textured quad over the whole window.
//
// The shader stays deliberately plain: one binding for the texture and one push
// constant that scales the quad, so the pipeline layout reflects a single
// descriptor set and a single push constant range.
//
// Sampling with an explicit LOD would need either a derivatives-capable stage
// or a forced LOD; the default implicit-LOD path keeps the device requirements
// at their minimum, exactly like the other samples.

struct PushData
{
    float2 scale;
    float2 uv_scale;
};

[[vk::push_constant]] PushData push_data;

[[vk::binding(0, 0)]] Sampler2D texture_0;

struct VSOut
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION,
               [[vk::location(1)]] float2 uv : TEXCOORD)
{
    VSOut output;
    output.position = float4(position * push_data.scale, 0.0, 1.0);
    output.uv = uv * push_data.uv_scale;
    return output;
}

[shader("fragment")]
float4 fragMain([[vk::location(0)]] float2 uv) : SV_Target
{
    return texture_0.Sample(uv);
}
