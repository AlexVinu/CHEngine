import common;

ConstantBuffer<CameraUBO>   camera;
ConstantBuffer<ObjectUBO>   object;
ConstantBuffer<LightingUBO> lighting;
ConstantBuffer<MaterialUBO> material;

struct VSIn
{
    [[vk::location(0)]] float3 Position  : POSITION;   // billboard offset in [-1,1]
    [[vk::location(1)]] float3 Normal    : NORMAL;     // unused
    [[vk::location(2)]] float2 TexCoords : TEXCOORD0;  // unused
    [[vk::location(3)]] float3 Color     : COLOR;
};

struct VSOut
{
    float4 Position     : SV_Position;
    float3 RayTarget    : TEXCOORD0;  // world-space billboard vertex (ray endpoint)
    float3 SphereCenter : TEXCOORD1;  // sphere center in world space
    float  WorldRadius  : TEXCOORD2;  // sphere world-space radius
    float3 Color        : COLOR;
};

struct PSOut
{
    float4 color : SV_Target;
    float  depth : SV_Depth;  // exact sphere-surface depth for correct occlusion
};

[shader("vertex")]
VSOut vertMain(VSIn input)
{
    VSOut o;

    // Sphere center: use the same matrix multiply path as the cube shader
    // (mul instead of direct column access) to guarantee correct GPU tracking.
    float3 sphereCenter = mul(object.Transform, float4(0.0, 0.0, 0.0, 1.0)).xyz;

    // World radius = scale * base mesh radius (0.5).
    float worldRadius = length(mul((float3x3)object.Transform, float3(1.0, 0.0, 0.0))) * 0.5f;

    // Camera-facing billboard orientation
    float3 camPos  = camera.CameraPos.xyz;
    float3 viewDir = normalize(sphereCenter - camPos);

    float3 worldUp = float3(0.0, 1.0, 0.0);
    float3 camRight, camUp;
    if (abs(dot(viewDir, worldUp)) > 0.999)
    {
        camRight = normalize(cross(float3(0.0, 0.0, 1.0), viewDir));
    }
    else
    {
        camRight = normalize(cross(worldUp, viewDir));
    }
    camUp = cross(viewDir, camRight);

    // Scale the billboard to exactly enclose the sphere's silhouette.
    // For perspective: silhouette radius (at sphere depth) = r * d / sqrt(d^2 - r^2)
    float dist = length(sphereCenter - camPos);
    float d2   = dist * dist - worldRadius * worldRadius;
    float billboardRadius = (d2 > 0.0001)
                          ? worldRadius * dist / sqrt(d2)
                          : worldRadius * 2.0;  // camera inside/very close to sphere

    float3 worldPos = sphereCenter
                    + input.Position.x * billboardRadius * camRight
                    + input.Position.y * billboardRadius * camUp;

    o.Position     = mul(camera.ViewProjection, float4(worldPos, 1.0));
    o.RayTarget    = worldPos;
    o.SphereCenter = sphereCenter;
    o.WorldRadius  = worldRadius;
    o.Color        = input.Color * object.Color.rgb;
    return o;
}

// ─── Lighting ─────────────────────────────────────────────────────────────────

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
        float  d       = length(toLight);
        lightDir       = toLight / max(d, 0.001);
        atten          = CalcAttenuation(d, lighting.Lights[index].Range);

        if (type == 2)
        {
            float theta      = dot(lightDir, normalize(-lighting.Lights[index].Direction.xyz));
            float eps        = lighting.Lights[index].InnerCone - lighting.Lights[index].OuterCone;
            float spotFactor = clamp((theta - lighting.Lights[index].OuterCone) / max(eps, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    float  diff    = max(dot(normal, lightDir), 0.0);
    float3 diffuse = diff * lightCol;

    float3 halfDir  = normalize(lightDir + viewDir);
    float  spec     = pow(max(dot(normal, halfDir), 0.0), max(material.Shininess, 1.0));
    float3 specular = 0.3 * spec * lightCol;

    return (diffuse + specular) * atten;
}

// ─── Fragment ─────────────────────────────────────────────────────────────────

[shader("fragment")]
PSOut fragMain(VSOut input)
{
    PSOut o;
    // Safe defaults: far-plane depth + transparent color.
    // If discard is ignored (some Metal configs), these prevent depth-buffer corruption.
    o.depth = 1.0f;
    o.color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 sphereCenter = input.SphereCenter;
    float  worldRadius  = input.WorldRadius;
    float3 rayOrigin    = camera.CameraPos.xyz;
    float3 rayDir       = normalize(input.RayTarget - rayOrigin);

    // Analytic ray-sphere intersection (a=1 since rayDir is normalised):
    //   t^2 + 2*b*t + c = 0
    float3 oc   = rayOrigin - sphereCenter;
    float  b    = dot(rayDir, oc);
    float  c    = dot(oc, oc) - worldRadius * worldRadius;
    float  disc = b * b - c;

    if (disc < 0.0f) { discard; return o; }

    float t = -b - sqrt(disc);
    if (t < 0.0f)   { discard; return o; }

    float3 hitPoint = rayOrigin + t * rayDir;
    float3 normal   = normalize(hitPoint - sphereCenter);
    float3 viewDir  = -rayDir;

    // Exact sphere-surface depth for correct Z-buffering
    float4 clipPos = mul(camera.ViewProjection, float4(hitPoint, 1.0f));
    o.depth = clipPos.z / clipPos.w;

    // Blinn-Phong shading
    float3 baseColor    = input.Color;
    float3 lightResult  = lighting.AmbientColor.rgb;
    int    count        = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lightResult += CalcLight(i, normal, hitPoint, viewDir);

    float3 color = lightResult * baseColor;

    // Fresnel rim highlight
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 4.0);
    color += rim * 0.15 * baseColor;

    if (object.Selected > 0.5)
        color = lerp(color, float3(1.0, 0.6, 0.0), 0.15);

    o.color = float4(color, object.Color.a);
    return o;
}
