#version 330 core

// ─── Максимум источников света (forward rendering) ────────────────────────
#define MAX_LIGHTS 8

in vec3 v_Normal;
in vec3 v_FragPos;
in vec3 v_Color;
in vec2 v_TexCoords;

out vec4 FragColor;

// ─── Материал объекта ─────────────────────────────────────────────────────
uniform vec4      u_Color;
uniform float     u_Selected;
uniform sampler2D u_DiffuseTexture;
uniform int       u_UseTexture;
uniform sampler2D u_SpecularMap;
uniform int       u_UseSpecularMap;
uniform float     u_Shininess;       // Blinn-Phong глянцевость (степень)

// ─── Камера ───────────────────────────────────────────────────────────────
uniform vec3 u_CameraPos;

// ─── Глобальный ambient ───────────────────────────────────────────────────
uniform vec3 u_AmbientColor;

// ─── Массив источников света ──────────────────────────────────────────────
// type: 0 = Directional, 1 = Point, 2 = Spot
uniform int   u_NumLights;
uniform int   u_LightType      [MAX_LIGHTS];
uniform vec3  u_LightPosition  [MAX_LIGHTS];
uniform vec3  u_LightDirection [MAX_LIGHTS];
uniform vec3  u_LightColor     [MAX_LIGHTS];
uniform float u_LightIntensity [MAX_LIGHTS];
uniform float u_LightRange     [MAX_LIGHTS];
uniform float u_LightInnerCone [MAX_LIGHTS]; // cos(angle)
uniform float u_LightOuterCone [MAX_LIGHTS]; // cos(angle)

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
    int   type     = u_LightType[index];
    vec3  lightCol = u_LightColor[index] * u_LightIntensity[index];
    float atten    = 1.0;

    vec3 lightDir;

    if (type == 0) {
        // Directional — направление фиксировано, без затухания
        lightDir = normalize(-u_LightDirection[index]);
    } else {
        // Point / Spot — от фрагмента к источнику
        vec3  toLight = u_LightPosition[index] - v_FragPos;
        float dist    = length(toLight);
        lightDir      = toLight / max(dist, 0.001);
        atten         = CalcAttenuation(dist, u_LightRange[index]);

        if (type == 2) {
            // Spot — конус
            float theta      = dot(lightDir, normalize(-u_LightDirection[index]));
            float epsilon    = u_LightInnerCone[index] - u_LightOuterCone[index];
            float spotFactor = clamp((theta - u_LightOuterCone[index]) / max(epsilon, 0.001), 0.0, 1.0);
            atten *= spotFactor;
        }
    }

    // Diffuse (Lambertian)
    float diff    = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = diff * lightCol;

    // Specular (Blinn-Phong) — модулируется specular map если есть
    vec3  halfDir = normalize(lightDir + viewDir);
    float spec    = pow(max(dot(normal, halfDir), 0.0), max(u_Shininess, 1.0));
    vec3  specular = 0.5 * spec * specularMask * lightCol;

    return (diffuse + specular) * atten;
}

void main()
{
    vec3 norm    = normalize(v_Normal);
    vec3 viewDir = normalize(u_CameraPos - v_FragPos);

    // Базовый цвет (диффузная текстура или vertex color)
    vec3 baseColor = (u_UseTexture > 0)
        ? texture(u_DiffuseTexture, v_TexCoords).rgb * u_Color.rgb
        : v_Color * u_Color.rgb;

    // Specular маска из текстуры (R-канал) или константа
    float specMask = (u_UseSpecularMap > 0)
        ? texture(u_SpecularMap, v_TexCoords).r
        : 1.0;

    // Суммируем вклад всех источников
    vec3 lighting = u_AmbientColor;
    int  count    = min(u_NumLights, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
        lighting += CalcLight(i, norm, viewDir, specMask);

    vec3 result = lighting * baseColor;

    // Подсветка выделения
    if (u_Selected > 0.5)
        result = mix(result, vec3(1.0, 0.6, 0.0), 0.15);

    FragColor = vec4(result, u_Color.a);
}
