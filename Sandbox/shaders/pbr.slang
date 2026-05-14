import common;

ConstantBuffer<CameraUBO>   camera;
ConstantBuffer<ObjectUBO>   object;
ConstantBuffer<LightingUBO> lighting;
ConstantBuffer<MaterialUBO> material;

Sampler2D u_DiffuseTexture;
Sampler2D u_SpecularMap;
Sampler2D u_NormalMap;
Sampler2D u_ORMmap;
Sampler2D u_EmissiveMap;

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

// ─── PBR: Cook-Torrance BRDF ─────────────────────────────────────────────

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159 * denom * denom, 0.0001);
}

float GeometrySchlickGGX(float NdV, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float CalcAttenuation(float dist, float range)
{
    float ratio   = dist / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target
{
    float3 N = normalize(input.Normal);
    float3 V = normalize(camera.CameraPos.xyz - input.FragPos);

    float roughness = material.Roughness;
    float metallic  = material.Metallic;
    float ao        = material.AO;

    // Sample ORM map if available
    if ((material.MaterialFlags & 4u) != 0u) // kPBR_UseORMmap
    {
        float3 orm = u_ORMmap.Sample(input.TexCoords).rgb;
        ao        = orm.r;
        roughness = orm.g;
        metallic  = orm.b;
    }

    float3 albedo = (material.UseTexture > 0)
        ? u_DiffuseTexture.Sample(input.TexCoords).rgb * object.Color.rgb
        : input.Color * object.Color.rgb;

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);

    int count = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
    {
        int    type     = lighting.Lights[i].Type;
        float3 lightCol = lighting.Lights[i].ColorIntensity.rgb
                        * lighting.Lights[i].ColorIntensity.a;
        float  atten    = 1.0;

        float3 L;
        if (type == 0)
        {
            L = normalize(-lighting.Lights[i].Direction.xyz);
        }
        else
        {
            float3 toLight = lighting.Lights[i].Position.xyz - input.FragPos;
            float  dist    = length(toLight);
            L              = toLight / max(dist, 0.001);
            atten          = CalcAttenuation(dist, lighting.Lights[i].Range);

            if (type == 2)
            {
                float theta      = dot(L, normalize(-lighting.Lights[i].Direction.xyz));
                float epsilon    = lighting.Lights[i].InnerCone - lighting.Lights[i].OuterCone;
                float spotFactor = clamp((theta - lighting.Lights[i].OuterCone) / max(epsilon, 0.001), 0.0, 1.0);
                atten *= spotFactor;
            }
        }

        float3 H = normalize(V + L);

        float NdL = max(dot(N, L), 0.0);
        float NdV = max(dot(N, V), 0.0);

        float3 radiance  = lightCol * atten;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;

        float3 numerator   = NDF * G * F;
        float  denominator = 4.0 * NdV * NdL + 0.0001;
        float3 specular    = numerator / denominator;

        float3 diffuse = kD * albedo / 3.14159;

        Lo += (diffuse + specular) * radiance * NdL;
    }

    float3 ambient = lighting.AmbientColor.rgb * albedo * ao;
    float3 color   = ambient + Lo;

    // Emissive
    if ((material.MaterialFlags & 8u) != 0u) // kPBR_UseEmissiveMap
        color += u_EmissiveMap.Sample(input.TexCoords).rgb;

    if (object.Selected > 0.5)
        color = lerp(color, float3(1.0, 0.6, 0.0), 0.15);

    return float4(color, object.Color.a);
}
