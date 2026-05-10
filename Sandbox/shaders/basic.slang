import common;

ConstantBuffer<CameraUBO> camera;
ConstantBuffer<ObjectUBO> object;

struct VSIn
{
    [[vk::location(0)]] float3 Position : POSITION;
    [[vk::location(1)]] float3 Color    : COLOR;
};

struct VSOut
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    o.Color    = input.Color;
    o.Position = mul(camera.ViewProjection, mul(object.Transform, float4(input.Position, 1.0)));
    return o;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    return float4(input.Color, 1.0) * object.Color;
}
