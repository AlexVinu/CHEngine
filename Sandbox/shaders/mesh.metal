#include <metal_stdlib>
using namespace metal;

#define MAX_LIGHTS 8

struct VertexIn {
    float3 position  [[attribute(0)]];
    float3 normal    [[attribute(1)]];
    float2 texCoords [[attribute(2)]];
    float3 color     [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 normal;
    float3 fragPos;
    float3 color;
    float2 texCoords;
};

struct VertexUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Transform;
    float4x4 u_NormalMatrix;
    float4x4 u_InvViewProj;
};

struct LightData {
    int    type;
    int    _pad0;
    int    _pad1;
    int    _pad2;
    float4 position;
    float4 direction;
    float4 colorIntensity;  // .rgb = color, .a = intensity
    float  range;
    float  innerCone;
    float  outerCone;
    float  _pad3;
};

struct FragmentUniforms {
    float4 u_Color;
    float4 u_CameraPos;
    float4 u_AmbientColor;
    int    u_NumLights;
    int    u_UseTexture;
    int    u_UseSpecularMap;
    float  u_Shininess;
    float  u_Selected;
    int    _pad0;
    int    _pad1;
    int    _pad2;
    LightData lights[MAX_LIGHTS];
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]],
                            constant VertexUniforms& u [[buffer(1)]])
{
    VertexOut out;
    float4 worldPos = u.u_Transform * float4(in.position, 1.0);
    out.fragPos     = worldPos.xyz;
    out.normal      = (u.u_NormalMatrix * float4(in.normal, 0.0)).xyz;
    out.color       = in.color;
    out.texCoords   = in.texCoords;
    out.position    = u.u_ViewProjection * worldPos;
    // GLM produces OpenGL NDC depth [-1,1]; Metal clips [0,1] → remap
    out.position.z  = (out.position.z + out.position.w) * 0.5;
    return out;
}

// ─── Attenuation ────────────────────────────────────────────────────────────
float CalcAttenuation(float distance, float range)
{
    float ratio   = distance / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

// ─── Light contribution ─────────────────────────────────────────────────────
float3 CalcLight(int index, float3 fragPos, float3 normal, float3 viewDir,
                 float specularMask, constant FragmentUniforms& u)
{
    int   type     = u.lights[index].type;
    float3 lightCol = u.lights[index].colorIntensity.rgb * u.lights[index].colorIntensity.a;
    float atten    = 1.0;

    float3 lightDir;

    if (type == 0) {
        // Directional
        lightDir = normalize(-u.lights[index].direction.xyz);
    } else {
        // Point / Spot
        float3 toLight = u.lights[index].position.xyz - fragPos;
        float  dist    = length(toLight);
        lightDir       = toLight / max(dist, 0.001);
        atten          = CalcAttenuation(dist, u.lights[index].range);

        if (type == 2) {
            // Spot cone
            float theta      = dot(lightDir, normalize(-u.lights[index].direction.xyz));
            float epsilon    = u.lights[index].innerCone - u.lights[index].outerCone;
            float spotFactor = clamp((theta - u.lights[index].outerCone) / max(epsilon, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    // Diffuse (Lambertian)
    float diff    = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diff * lightCol;

    // Specular (Blinn-Phong)
    float3 halfDir = normalize(lightDir + viewDir);
    float  spec    = pow(max(dot(normal, halfDir), 0.0), max(u.u_Shininess, 1.0));
    float3 specular = 0.5 * spec * specularMask * lightCol;

    return (diffuse + specular) * atten;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             constant FragmentUniforms& u [[buffer(0)]],
                             texture2d<float> diffuseMap [[texture(0)]],
                             texture2d<float> specularMap [[texture(1)]],
                             sampler texSampler [[sampler(0)]])
{
    float3 norm    = normalize(in.normal);
    float3 viewDir = normalize(u.u_CameraPos.xyz - in.fragPos);

    // Base color
    float3 baseColor;
    if (u.u_UseTexture > 0)
        baseColor = diffuseMap.sample(texSampler, in.texCoords).rgb * u.u_Color.rgb;
    else
        baseColor = in.color * u.u_Color.rgb;

    // Specular mask
    float specMask;
    if (u.u_UseSpecularMap > 0)
        specMask = specularMap.sample(texSampler, in.texCoords).r;
    else
        specMask = 1.0;

    // Accumulate lighting
    float3 lighting = u.u_AmbientColor.rgb;
    int count = min(u.u_NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lighting += CalcLight(i, in.fragPos, norm, viewDir, specMask, u);

    float3 result = lighting * baseColor;

    // Selection highlight
    if (u.u_Selected > 0.5)
        result = mix(result, float3(1.0, 0.6, 0.0), 0.15);

    return float4(result, u.u_Color.a);
}
