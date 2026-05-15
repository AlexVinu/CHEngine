import common;
import lighting;

ConstantBuffer<CameraUBO>   camera;
ConstantBuffer<ObjectUBO>   object;
ConstantBuffer<LightingUBO> lighting;
ConstantBuffer<MaterialUBO> material;

// Sphere center in world space is extracted from object.Transform[12..14]
// Analytic normal = normalize(fragPos - sphereCenter), giving pixel-perfect
// normals regardless of mesh tessellation.

struct VSIn
{
    [[vk::location(0)]] float3 Position  : POSITION;
    [[vk::location(1)]] float3 Normal    : NORMAL;
    [[vk::location(2)]] float2 TexCoords : TEXCOORD0;
    [[vk::location(3)]] float3 Color     : COLOR;
};

struct VSOut
{
    float4 Position  : SV_Position;
    float3 Normal    : NORMAL;
    float3 FragPos   : TEXCOORD1;
    float3 Color     : COLOR;
    float2 TexCoords : TEXCOORD0;
    float3 SphereCenter : TEXCOORD2;
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;
    float4 worldPos = mul(object.Transform, float4(input.Position, 1.0));
    o.FragPos       = worldPos.xyz;
    o.Normal        = mul((float3x3)object.NormalMatrix, input.Normal);
    o.Color         = input.Color;
    o.TexCoords     = input.TexCoords;
    o.Position      = mul(camera.ViewProjection, worldPos);

    // Compute sphere center in world space from the model matrix
    // Transform is column-major: column[3] = translation
    o.SphereCenter = object.Transform[3].xyz;
    return o;
}


[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    // Analytic normal: pixel-perfect smoothness regardless of tessellation level
    float3 norm = normalize(input.FragPos - input.SphereCenter);

    float3 viewDir = normalize(camera.CameraPos.xyz - input.FragPos);

    float3 baseColor = input.Color * object.Color.rgb;

    float3 lightingResult = lighting.AmbientColor.rgb;
    int    count          = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lightingResult += CalcLight(i, norm, input.FragPos, viewDir, lighting, material);

    float3 result = lightingResult * baseColor;

    // Fresnel rim light for spheres
    float  rim    = 1.0 - max(dot(viewDir, norm), 0.0);
    float3 rimCol = rim * rim * float3(0.3, 0.4, 0.6);
    result += rimCol * 0.3;

    if (object.Selected > 0.5)
        result = lerp(result, float3(1.0, 0.6, 0.0), 0.15);

    return float4(result, object.Color.a);
}
