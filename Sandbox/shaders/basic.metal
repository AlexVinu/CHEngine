#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 color    [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 color;
};

struct VertexUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Transform;
    float4x4 u_NormalMatrix;
    float4x4 u_InvViewProj;
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
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]],
                            constant VertexUniforms& u [[buffer(1)]])
{
    VertexOut out;
    out.color    = in.color;
    out.position = u.u_ViewProjection * u.u_Transform * float4(in.position, 1.0);
    // GLM produces OpenGL NDC depth [-1,1]; Metal clips [0,1] → remap
    out.position.z = (out.position.z + out.position.w) * 0.5;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             constant FragmentUniforms& u [[buffer(0)]])
{
    return float4(in.color, 1.0) * u.u_Color;
}
