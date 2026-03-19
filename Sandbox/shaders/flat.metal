#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float3 position [[attribute(0)]];
};

struct VertexOut {
    float4 position [[position]];
};

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
    int      UseTexture;
    int      UseSpecularMap;
    float    Shininess;
};

vertex VertexOut vertexMain(VertexIn in [[stage_in]],
                            constant CameraUBO& camera [[buffer(1)]],
                            constant ObjectUBO& object [[buffer(2)]])
{
    VertexOut out;
    out.position = camera.ViewProjection * object.Transform * float4(in.position, 1.0);
    // GLM produces OpenGL NDC depth [-1,1]; Metal clips [0,1] → remap
    out.position.z = (out.position.z + out.position.w) * 0.5;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]])
{
    return float4(1.0, 0.5, 0.0, 1.0);
}
