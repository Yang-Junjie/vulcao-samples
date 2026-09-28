// Pass 1 of the 06_two_pass sample: lights the teapot with the Blinn-Phong
// model and renders it into a linear HDR offscreen target.
//
// The per-frame state (camera, light) lives in a uniform buffer, while the
// per-object state (model matrix, material) arrives as push constants. That
// split mirrors how engines usually feed shaders: a small constant buffer per
// frame, and tiny per-draw state through push constants.

struct SceneUniforms
{
    float4x4 view;
    float4x4 projection;
    float4 camera_position;
    float4 light_direction; // xyz points from the surface toward the light
    float4 light_color;     // rgb color, a holds the intensity
};

[[vk::binding(0, 0)]] ConstantBuffer<SceneUniforms> uniforms;

struct PushData
{
    float4x4 model;
    float4 base_color;
    float4 material; // x ambient, y diffuse, z specular, w shininess
};

[[vk::push_constant]] PushData push_data;

struct VSOut
{
    float4 position : SV_Position;
    float3 world_position : TEXCOORD0;
    float3 world_normal : NORMAL;
};

[shader("vertex")]
VSOut vertMain([[vk::location(0)]] float3 position : POSITION,
               [[vk::location(1)]] float3 normal : NORMAL)
{
    VSOut output;
    const float4 world = mul(push_data.model, float4(position, 1.0));
    output.position = mul(uniforms.projection, mul(uniforms.view, world));
    output.world_position = world.xyz;
    // The model matrix is a rotation, so it transforms normals directly.
    output.world_normal = mul((float3x3)push_data.model, normal);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    const float3 n = normalize(input.world_normal);
    const float3 l = normalize(uniforms.light_direction.xyz);
    const float3 v = normalize(uniforms.camera_position.xyz - input.world_position);
    const float3 h = normalize(l + v);

    const float3 light = uniforms.light_color.rgb * uniforms.light_color.a;
    const float3 base = push_data.base_color.rgb;

    // Blinn-Phong: ambient + diffuse + specular, all in linear space so the
    // HDR target can keep values above 1 for the post pass to tonemap.
    const float n_dot_l = max(dot(n, l), 0.0);
    const float n_dot_h = max(dot(n, h), 0.0);
    const float specular = n_dot_l > 0.0 ? pow(n_dot_h, push_data.material.w) : 0.0;

    const float3 ambient = base * light * push_data.material.x;
    const float3 diffuse = base * light * (push_data.material.y * n_dot_l);
    const float3 spec = light * (push_data.material.z * specular);

    return float4(ambient + diffuse + spec, push_data.base_color.a);
}
