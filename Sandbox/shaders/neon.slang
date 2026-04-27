import common;

ConstantBuffer<CameraUBO> camera;
ConstantBuffer<ObjectUBO> object;

struct VSIn
{
    float3 Position : POSITION;
};

struct VSOut
{
    float4 Position    : SV_Position;
    float3 LocalPos    : TEXCOORD0;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    o.LocalPos = input.Position;
    o.Position = mul(camera.ViewProjection, mul(object.Transform, float4(input.Position, 1.0)));
    return o;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    float t = clamp(input.LocalPos.y * 0.5 + 0.5, 0.0, 1.0);
    return float4(0.0, t, 1.0, 1.0);
}
