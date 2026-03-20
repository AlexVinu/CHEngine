#version 330 core

// ─── Максимум источников света (forward rendering) ────────────────────────
#define MAX_LIGHTS 8

in vec3 v_Normal;
in vec3 v_FragPos;
in vec3 v_Color;
in vec2 v_TexCoords;

out vec4 FragColor;

// Texture sampler'ы — нельзя поместить в UBO, остаются plain uniform
uniform sampler2D u_DiffuseTexture;
uniform sampler2D u_SpecularMap;

// ─── UBO: камера ─────────────────────────────────────────────────────────
layout(std140) uniform CameraUBO
{
    mat4 ViewProjection;
    mat4 InvViewProj;
    vec4 CameraPos;
} camera;

// ─── UBO: объект ─────────────────────────────────────────────────────────
layout(std140) uniform ObjectUBO
{
    mat4  Transform;
    mat4  NormalMatrix;
    vec4  Color;
    float Selected;
    int   UseTexture;
    int   UseSpecularMap;
    float Shininess;
} object;

// ─── UBO: освещение ──────────────────────────────────────────────────────
struct LightData
{
    int  Type;
    vec4 Position;
    vec4 Direction;
    vec4 ColorIntensity;
    float Range;
    float InnerCone;
    float OuterCone;
};

layout(std140) uniform LightingUBO
{
    vec4      AmbientColor;
    int       NumLights;
    LightData Lights[MAX_LIGHTS];
} lighting;

// ─── Расчёт затухания для Point/Spot ──────────────────────────────────────
float CalcAttenuation(float distance, float range)
{
    float ratio   = distance / max(range, 0.001);
    float clamped = clamp(1.0 - ratio * ratio, 0.0, 1.0);
    return clamped * clamped;
}

// ─── Вычисление вклада одного источника ───────────────────────────────────
vec3 CalcLight(int index, vec3 normal, vec3 viewDir, float specularMask)
{
    int   type     = lighting.Lights[index].Type;
    vec3  lightCol = lighting.Lights[index].ColorIntensity.rgb
                   * lighting.Lights[index].ColorIntensity.a;
    float atten    = 1.0;

    vec3 lightDir;

    if (type == 0) {
        // Directional — направление фиксировано, без затухания
        lightDir = normalize(-lighting.Lights[index].Direction.xyz);
    } else {
        // Point / Spot — от фрагмента к источнику
        vec3  toLight = lighting.Lights[index].Position.xyz - v_FragPos;
        float dist    = length(toLight);
        lightDir      = toLight / max(dist, 0.001);
        atten         = CalcAttenuation(dist, lighting.Lights[index].Range);

        if (type == 2) {
            // Spot — конус
            float theta      = dot(lightDir, normalize(-lighting.Lights[index].Direction.xyz));
            float epsilon    = lighting.Lights[index].InnerCone - lighting.Lights[index].OuterCone;
            float spotFactor = clamp((theta - lighting.Lights[index].OuterCone) / max(epsilon, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    // Diffuse (Lambertian)
    float diff    = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = diff * lightCol;

    // Specular (Blinn-Phong) — модулируется specular map если есть
    vec3  halfDir = normalize(lightDir + viewDir);
    float spec    = pow(max(dot(normal, halfDir), 0.0), max(object.Shininess, 1.0));
    vec3  specular = 0.5 * spec * specularMask * lightCol;

    return (diffuse + specular) * atten;
}

void main()
{
    vec3 norm    = normalize(v_Normal);
    vec3 viewDir = normalize(camera.CameraPos.xyz - v_FragPos);

    // Базовый цвет (диффузная текстура или vertex color)
    vec3 baseColor = (object.UseTexture > 0)
        ? texture(u_DiffuseTexture, v_TexCoords).rgb * object.Color.rgb
        : v_Color * object.Color.rgb;

    // Specular маска из текстуры (R-канал) или константа
    float specMask = (object.UseSpecularMap > 0)
        ? texture(u_SpecularMap, v_TexCoords).r
        : 1.0;

    // Суммируем вклад всех источников
    vec3 lighting_result = lighting.AmbientColor.rgb;
    int  count           = min(lighting.NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lighting_result += CalcLight(i, norm, viewDir, specMask);

    vec3 result = lighting_result * baseColor;

    // Подсветка выделения
    if (object.Selected > 0.5)
        result = mix(result, vec3(1.0, 0.6, 0.0), 0.15);

    FragColor = vec4(result, object.Color.a);
}
