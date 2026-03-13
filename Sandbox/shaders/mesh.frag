#version 330 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec3 v_Color;
in vec2 v_TexCoords;

out vec4 FragColor;

uniform vec4      u_Color;
uniform vec3      u_LightDir;
uniform vec3      u_LightColor;
uniform vec3      u_AmbientColor;
uniform float     u_Selected;
uniform sampler2D u_DiffuseTexture;
uniform int       u_UseTexture;

void main()
{
    vec3 norm     = normalize(v_Normal);
    vec3 lightDir = normalize(-u_LightDir);

    float diff   = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;
    vec3 ambient = u_AmbientColor;

    vec3 baseColor = (u_UseTexture > 0)
        ? texture(u_DiffuseTexture, v_TexCoords).rgb * u_Color.rgb
        : v_Color * u_Color.rgb;

    vec3 result = (ambient + diffuse) * baseColor;

    if (u_Selected > 0.5)
        result = mix(result, vec3(1.0, 0.6, 0.0), 0.15);

    FragColor = vec4(result, u_Color.a);
}
