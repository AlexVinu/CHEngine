#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 localPos;
};

struct VertexUniforms {
    float4x4 u_ViewProjection;
    float4x4 u_Transform;
    float4x4 u_NormalMatrix;
    float4x4 u_InvViewProj;
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]],
                            constant VertexUniforms& u [[buffer(1)]])
{
    VertexOut out;
    out.localPos = in.position;
    out.position = u.u_ViewProjection * u.u_Transform * float4(in.position, 1.0);
    // GLM produces OpenGL NDC depth [-1,1]; Metal clips [0,1] → remap
    out.position.z = (out.position.z + out.position.w) * 0.5;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]])
{
    // Vertical blue-to-cyan gradient
    float t = clamp(in.localPos.y * 0.5 + 0.5, 0.0, 1.0);
    return float4(0.0, t, 1.0, 1.0);
}
