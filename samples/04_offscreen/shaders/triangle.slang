// Draws one triangle into an offscreen image.
//
// The 04_offscreen sample reads that image back and checks its pixels, so the
// colour arrives as a push constant and the CPU side knows exactly which values
// to expect.
//
// The positions come from a vertex buffer rather than SV_VertexID on purpose:
// Slang lowers SV_VertexID to gl_VertexIndex plus BaseVertex, and BaseVertex
// needs the SPIR-V DrawParameters capability, which in turn needs
// DeviceFeatures::shader_draw_parameters. A vertex buffer keeps this sample's
// device requirements at their minimum.

struct PushData
{
    float4 color;
};

[[vk::push_constant]] PushData push_data;

struct VSOut
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float3 position : POSITION)
{
    VSOut output;
    output.position = float4(position, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return push_data.color;
}
