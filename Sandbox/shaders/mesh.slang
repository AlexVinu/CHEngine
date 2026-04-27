import common;

ConstantBuffer<CameraUBO>   camera;
ConstantBuffer<ObjectUBO>   object;
ConstantBuffer<LightingUBO> lighting;
ConstantBuffer<MaterialUBO> material;

Sampler2D u_DiffuseTexture;
Sampler2D u_SpecularMap;

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
    return o;
}

float CalcAttenuation(float dist, float range)
{
    float ratio   = dist / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

float3 CalcLight(int index, float3 normal, float3 fragPos, float3 viewDir, float specularMask)
{
    int    type     = lighting.Lights[index].Type;
    float3 lightCol = lighting.Lights[index].ColorIntensity.rgb
                    * lighting.Lights[index].ColorIntensity.a;
    float  atten    = 1.0;

    float3 lightDir;
    if (type == 0)
    {
        // Directional
        lightDir = normalize(-lighting.Lights[index].Direction.xyz);
    }
    else
    {
        // Point / Spot
        float3 toLight = lighting.Lights[index].Position.xyz - fragPos;
        float  dist    = length(toLight);
        lightDir       = toLight / max(dist, 0.001);
        atten          = CalcAttenuation(dist, lighting.Lights[index].Range);

        if (type == 2)
        {
            float theta      = dot(lightDir, normalize(-lighting.Lights[index].Direction.xyz));
            float epsilon    = lighting.Lights[index].InnerCone - lighting.Lights[index].OuterCone;
            float spotFactor = clamp((theta - lighting.Lights[index].OuterCone) / max(epsilon, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    float diff    = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diff * lightCol;

    float3 halfDir  = normalize(lightDir + viewDir);
    float  spec     = pow(max(dot(normal, halfDir), 0.0), max(material.Shininess, 1.0));
    float3 specular = 0.5 * spec * specularMask * lightCol * material.SpecularScale;

    return (diffuse + specular) * atten;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    float3 norm    = normalize(input.Normal);
    float3 viewDir = normalize(camera.CameraPos.xyz - input.FragPos);

    float3 baseColor = (material.UseTexture > 0)
        ? u_DiffuseTexture.Sample(input.TexCoords).rgb * object.Color.rgb
        : input.Color * object.Color.rgb;

    float specMask = (material.UseSpecularMap > 0)
        ? u_SpecularMap.Sample(input.TexCoords).r
        : 1.0;

    float3 lightingResult = lighting.AmbientColor.rgb;
    int    count          = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lightingResult += CalcLight(i, norm, input.FragPos, viewDir, specMask);

    float3 result = lightingResult * baseColor;

    if (object.Selected > 0.5)
        result = lerp(result, float3(1.0, 0.6, 0.0), 0.15);

    return float4(result, object.Color.a);
}
