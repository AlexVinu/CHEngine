import common;

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

float CalcAttenuation(float dist, float range)
{
    float ratio   = dist / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

float3 CalcLight(int index, float3 normal, float3 fragPos, float3 viewDir)
{
    int    type     = lighting.Lights[index].Type;
    float3 lightCol = lighting.Lights[index].ColorIntensity.rgb
                    * lighting.Lights[index].ColorIntensity.a;
    float  atten    = 1.0;

    float3 lightDir;
    if (type == 0)
    {
        lightDir = normalize(-lighting.Lights[index].Direction.xyz);
    }
    else
    {
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
    float3 specular = 0.5 * spec * lightCol * material.SpecularScale;

    return (diffuse + specular) * atten;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    // Analytic normal: re-project from sphere center for perfect smoothness
    float3 sphereNormal = normalize(input.FragPos - input.SphereCenter);
    float3 norm         = normalize(lerp(sphereNormal, normalize(input.Normal), 0.3));

    float3 viewDir = normalize(camera.CameraPos.xyz - input.FragPos);

    float3 baseColor = input.Color * object.Color.rgb;

    float3 lightingResult = lighting.AmbientColor.rgb;
    int    count          = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lightingResult += CalcLight(i, norm, input.FragPos, viewDir);

    float3 result = lightingResult * baseColor;

    // Fresnel rim light for spheres
    float  rim    = 1.0 - max(dot(viewDir, norm), 0.0);
    float3 rimCol = rim * rim * float3(0.3, 0.4, 0.6);
    result += rimCol * 0.3;

    if (object.Selected > 0.5)
        result = lerp(result, float3(1.0, 0.6, 0.0), 0.15);

    return float4(result, object.Color.a);
}
