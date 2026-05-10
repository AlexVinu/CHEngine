import common;

ConstantBuffer<CameraUBO> camera;
ConstantBuffer<ObjectUBO> object;

struct VSIn
{
    float3 Position : POSITION;
};

struct VSOut
{
    float4 Position : SV_Position;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    o.Position = mul(camera.ViewProjection, mul(object.Transform, float4(input.Position, 1.0)));
    return o;
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return float4(1.0, 0.5, 0.0, 1.0);
}
