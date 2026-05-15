import common;

// Shared Blinn-Phong lighting functions used by sphere.slang and sphere_impostor.slang.
// Reads from the global `lighting` and `material` constant buffers declared in the
// importing shader.

float CalcAttenuation(float dist, float range)
{
    float ratio   = dist / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

float3 CalcLight(int index, float3 normal, float3 fragPos, float3 viewDir,
                 LightingUBO lighting, MaterialUBO material)
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

    float  diff    = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diff * lightCol;

    float3 halfDir  = normalize(lightDir + viewDir);
    float  spec     = pow(max(dot(normal, halfDir), 0.0), max(material.Shininess, 1.0));
    float3 specular = 0.5 * spec * lightCol * material.SpecularScale;

    return (diffuse + specular) * atten;
}
