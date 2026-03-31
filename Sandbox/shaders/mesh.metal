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

// ─── UBO-блоки (раскладка совпадает с OpenGL std140) ─────────────────────

struct CameraUBO {
    float4x4 ViewProjection;
    float4x4 InvViewProj;
    float4   CameraPos;
};

struct ObjectUBO {
    float4x4 Transform;
    float4x4 NormalMatrix;
    float4   Color;
    float    Selected;
    float    _pad0;
    float    _pad1;
    float    _pad2;
};

struct MaterialUBO {
    int   UseTexture;
    int   UseSpecularMap;
    float Shininess;
    uint  MaterialFlags;
    float SpecularScale;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct LightData {
    int    Type;
    int    _pad0;
    int    _pad1;
    int    _pad2;
    float4 Position;
    float4 Direction;
    float4 ColorIntensity;  // .rgb = color, .a = intensity
    float  Range;
    float  InnerCone;
    float  OuterCone;
    float  _pad3;
};

struct LightingUBO {
    float4    AmbientColor;
    int       NumLights;
    int       _pad0;
    int       _pad1;
    int       _pad2;
    LightData Lights[MAX_LIGHTS];
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]],
                            constant CameraUBO& camera [[buffer(1)]],
                            constant ObjectUBO& object [[buffer(2)]])
{
    VertexOut out;
    float4 worldPos = object.Transform * float4(in.position, 1.0);
    out.fragPos     = worldPos.xyz;
    out.normal      = (object.NormalMatrix * float4(in.normal, 0.0)).xyz;
    out.color       = in.color;
    out.texCoords   = in.texCoords;
    out.position    = camera.ViewProjection * worldPos;
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
                 float specularMask, constant LightingUBO& lighting,
                 float shininess, float specularScale)
{
    int   type     = lighting.Lights[index].Type;
    float3 lightCol = lighting.Lights[index].ColorIntensity.rgb * lighting.Lights[index].ColorIntensity.a;
    float atten    = 1.0;

    float3 lightDir;

    if (type == 0) {
        // Directional
        lightDir = normalize(-lighting.Lights[index].Direction.xyz);
    } else {
        // Point / Spot
        float3 toLight = lighting.Lights[index].Position.xyz - fragPos;
        float  dist    = length(toLight);
        lightDir       = toLight / max(dist, 0.001);
        atten          = CalcAttenuation(dist, lighting.Lights[index].Range);

        if (type == 2) {
            // Spot cone
            float theta      = dot(lightDir, normalize(-lighting.Lights[index].Direction.xyz));
            float epsilon    = lighting.Lights[index].InnerCone - lighting.Lights[index].OuterCone;
            float spotFactor = clamp((theta - lighting.Lights[index].OuterCone) / max(epsilon, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    // Diffuse (Lambertian)
    float diff    = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diff * lightCol;

    // Specular (Blinn-Phong)
    float3 halfDir = normalize(lightDir + viewDir);
    float  spec    = pow(max(dot(normal, halfDir), 0.0), max(shininess, 1.0));
    float3 specular = 0.5 * spec * specularMask * lightCol * specularScale;

    return (diffuse + specular) * atten;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             constant CameraUBO&    camera   [[buffer(1)]],
                             constant ObjectUBO&    object   [[buffer(2)]],
                             constant LightingUBO&  lighting [[buffer(3)]],
                             constant MaterialUBO&  material [[buffer(4)]],
                             texture2d<float> diffuseMap  [[texture(0)]],
                             texture2d<float> specularMap [[texture(1)]],
                             sampler texSampler [[sampler(0)]])
{
    float3 norm    = normalize(in.normal);
    float3 viewDir = normalize(camera.CameraPos.xyz - in.fragPos);

    // Base color
    float3 baseColor;
    if (material.UseTexture > 0)
        baseColor = diffuseMap.sample(texSampler, in.texCoords).rgb * object.Color.rgb;
    else
        baseColor = in.color * object.Color.rgb;

    // Specular mask
    float specMask;
    if (material.UseSpecularMap > 0)
        specMask = specularMap.sample(texSampler, in.texCoords).r;
    else
        specMask = 1.0;

    // Accumulate lighting
    float3 result_lighting = lighting.AmbientColor.rgb;
    int count = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        result_lighting += CalcLight(i, in.fragPos, norm, viewDir, specMask, lighting, material.Shininess, material.SpecularScale);

    float3 result = result_lighting * baseColor;

    // Selection highlight
    if (object.Selected > 0.5)
        result = mix(result, float3(1.0, 0.6, 0.0), 0.15);

    return float4(result, object.Color.a);
}
