#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
};

struct VertexOut {
    float4 position [[position]];
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
    out.position = u.u_ViewProjection * u.u_Transform * float4(in.position, 1.0);
    // GLM produces OpenGL NDC depth [-1,1]; Metal clips [0,1] → remap
    out.position.z = (out.position.z + out.position.w) * 0.5;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]])
{
    return float4(1.0, 0.5, 0.0, 1.0);
}
