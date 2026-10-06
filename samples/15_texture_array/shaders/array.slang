[[vk::binding(0, 0)]] Texture2DArray<float4> layers;
[[vk::binding(1, 0)]] SamplerState nearest_sampler;
struct LayerData { uint layer; };
[[vk::push_constant]] LayerData selection;

struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float2 position : POSITION)
{
    VSOut output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = position * 0.5 + 0.5;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    return layers.SampleLevel(nearest_sampler, float3(input.uv, float(selection.layer)), 0.0);
}
