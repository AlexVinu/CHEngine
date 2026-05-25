
struct CameraUBO
{
    float4x4 ViewProjection;
    float4x4 InvViewProj;
    float4   CameraPos;
    // NDC convention helpers (API-specific, set by RenderSystem):
    //   NDCNearZ      : -1.0 (OpenGL/Metal)  or  0.0 (Vulkan)
    //   NDCDepthScale :  0.5 (OpenGL/Metal)  or  1.0 (Vulkan)
    //   NDCDepthBias  :  0.5 (OpenGL/Metal)  or  0.0 (Vulkan)
    float    NDCNearZ;
    float    NDCDepthScale;
    float    NDCDepthBias;
    float    _camPad;
};

struct ObjectUBO
{
    float4x4 Transform;
    float4x4 NormalMatrix;
    float4   Color;
    float    Selected;
    float    _opad0;
    float    _opad1;
    float    _opad2;
};

struct MaterialUBO
{
    int   UseTexture;
    int   UseSpecularMap;
    float Shininess;
    uint  MaterialFlags;
    float SpecularScale;
    float Roughness;
    float Metallic;
    float AO;
};

struct LightData
{
    int    Type;
    int    _ldpad0;
    int    _ldpad1;
    int    _ldpad2;
    float4 Position;
    float4 Direction;
    float4 ColorIntensity;
    float  Range;
    float  InnerCone;
    float  OuterCone;
    float  _ldpad3;
};

static const int MAX_LIGHTS = 8;

struct LightingUBO
{
    float4    AmbientColor;
    int       NumLights;
    int       _lupad0;
    int       _lupad1;
    int       _lupad2;
    LightData Lights[MAX_LIGHTS];
};
